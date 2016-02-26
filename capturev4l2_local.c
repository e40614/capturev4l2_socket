#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <netinet/in.h>
#include <netdb.h> 
#include <signal.h>
#include "cv.h"
#include <sys/time.h>
#include <unistd.h>// sleep(3);
#include <sys/timeb.h>//timeb
#define CLEAR(x) memset(&(x), 0, sizeof(x))
int width;
int height;
// int realwidth;
// int realheight;

struct buffer {
		void   *start;
		size_t  length;
};

int n_buffers;  
struct buffer    *buffers;
// uint8_t *buffer;
uint32_t buf_len;
int camerafd;
char camera_device[30];

long long getSystemTime() {
		struct timeb t;
		ftime(&t);
		return 1000 * t.time + t.millitm;
}

static int xioctl(int fd, int request, void *arg)
{
		int r;

		do r = ioctl (fd, request, arg);
		while (-1 == r && EINTR == errno);

		return r;
}

int print_caps(int fd)
{
		struct v4l2_capability caps = {};
		if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
		{
				perror("Querying Capabilities");
				return 1;
		}

		printf( "Driver Caps:\n"
						"  Driver: \"%s\"\n"
						"  Card: \"%s\"\n"
						"  Bus: \"%s\"\n"
						"  Version: %d.%d\n"
						"  Capabilities: %08x\n",
						caps.driver,
						caps.card,
						caps.bus_info,
						(caps.version>>16)&&0xff,
						(caps.version>>24)&&0xff,
						caps.capabilities);


		struct v4l2_cropcap cropcap = {0};
		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_CROPCAP, &cropcap))
		{
				perror("Querying Cropping Capabilities");
				return 1;
		}

		printf( "Camera Cropping:\n"
						"  Bounds: %dx%d+%d+%d\n"
						"  Default: %dx%d+%d+%d\n"
						"  Aspect: %d/%d\n",
						cropcap.bounds.width, cropcap.bounds.height, cropcap.bounds.left, cropcap.bounds.top,
						cropcap.defrect.width, cropcap.defrect.height, cropcap.defrect.left, cropcap.defrect.top,
						cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);

		int support_grbg10 = 0;

		struct v4l2_fmtdesc fmtdesc = {0};
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		char fourcc[5] = {0};
		char c, e;
		printf("  FMT : CE Desc\n--------------------\n");
		while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
		{
				strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
				if (fmtdesc.pixelformat == V4L2_PIX_FMT_SGRBG10)
						support_grbg10 = 1;
				c = fmtdesc.flags & 1? 'C' : ' ';
				e = fmtdesc.flags & 2? 'E' : ' ';
				printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
				fmtdesc.index++;
		}
		/*
		   if (!support_grbg10)
		   {
		   printf("Doesn't support GRBG10.\n");
		   return 1;
		   }*/

		struct v4l2_format fmt = {0};
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = width;
		fmt.fmt.pix.height = height;
		//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
		//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
		fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

		if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		{
				perror("Setting Pixel Format");
				return 1;
		}
		int n;
		strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
		printf( "Selected Camera Mode:\n"
						"  Width: %d\n"
						"  Height: %d\n"
						"  PixFmt: %s\n"
						"  Field: %d\n",
						fmt.fmt.pix.width,
						fmt.fmt.pix.height,
						fourcc,
						fmt.fmt.pix.field);
		// n = write(sockfd,(char*)(&(fmt.fmt.pix.width)),4);
		// if (n < 0) 
		//  printf("ERROR writing to socket");
		// n = write(sockfd,(char*)(&(fmt.fmt.pix.height)),4);
		// if (n < 0) 
		//  printf("ERROR writing to socket");
		return 0;
}

int init_mmap(int fd)
{
		struct v4l2_requestbuffers req = {0};
		req.count = 4;
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
		{
				perror("Requesting Buffer");
				return 1;
		}
		if (req.count < 2) {
				fprintf(stderr, "Insufficient buffer memory on %s\n",
								camera_device);
				exit(EXIT_FAILURE);
		}

		buffers = calloc(req.count, sizeof(*buffers));
		if (!buffers) {
				fprintf(stderr, "Out of memory\n");
				exit(EXIT_FAILURE);
		}
		struct v4l2_buffer buf;
		for(n_buffers = 0; n_buffers < req.count; ++n_buffers){


				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = n_buffers;

				if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
				{
						perror("Querying Buffer");
						return 1;
				}
				buffers[n_buffers].length = buf.length;


				buffers[n_buffers].start = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
				buf_len = buf.length;
				printf("buffer%d => Length: %d\nAddress: %p\n", n_buffers, buf.length, buffers[n_buffers].start);
				// printf("Image Length: %d\n", buf.bytesused);
		}



		// int n = write(sockfd,(char*)(&(buf.length)),4);
		// if (n < 0) 
		//      printf("ERROR writing to socket");

		return 0;
}

int read_frame(int fd){
		struct v4l2_buffer buf;
		unsigned int i;

}


int capture_and_send_image(int fd)
{

		enum v4l2_buf_type type;
		int frame_num = 0;
		char camActive = 0;
		char rec;
		struct v4l2_buffer buf = {0};
		int n,i;
		char *bufferptr;
		int framesize;
		long long start, end;


		for (i = 0; i < n_buffers; ++i) {
				struct v4l2_buffer buf;

				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
						printf("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
				printf("VIDIOC_STREAMON");

		// buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		// buf.memory = V4L2_MEMORY_MMAP;
		// buf.index = 0;
		cvNamedWindow("window",CV_WINDOW_AUTOSIZE);
		while(1){
				if(frame_num == 0){
						start = getSystemTime();
				}

				fd_set fds;
				struct timeval tv;
				int r;

				FD_ZERO(&fds);
				FD_SET(fd, &fds);

				tv.tv_sec = 2;
				tv.tv_usec = 0;

				r = select(fd+1, &fds, NULL, NULL, &tv);
				if(-1 == r)
				{
						perror("Waiting for Frame");
						return 1;
				}
				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;

				if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
				{
						switch (errno) {
								case EAGAIN:
										continue;

								case EIO:
										/* Could ignore EIO, see spec. */

										/* fall through */

								default:
										perror("VIDIOC_DQBUF");
						}
				}
				assert(buf.index < n_buffers);
				IplImage* frame;
				CvMat cvmat = cvMat(height, width, CV_8UC3, (void*)buffers[buf.index].start);
				frame = cvDecodeImage(&cvmat, 1);

				cvShowImage("window", frame);
				cvReleaseImage(&frame);
				cvWaitKey(1);

				if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
						printf("VIDIOC_QBUF");  

				frame_num++;
				if(frame_num == 10){
						end = getSystemTime();
						frame_num = 0;
						printf("\rFPS:%3lld", 10000/(end - start));
						fflush(stdout);
				}
		}
		return 0;
}
void ctrlC(int sig){ // can be called asynchronously
		// if (-1 == munmap(buffer, buf_len))
		//         fprintf(stderr,"munmap");
		// if (-1 == close(camerafd))
		//         fprintf(stderr,"close");
		// printf("ByeBye!!\n");
		exit(0);
}
int main(int argc, char *argv[])
{
		strcpy(camera_device,"/dev/video");
		// char camera_device[30] = "/dev/video";
		int fd;

		struct sockaddr_in serv_addr;
		struct hostent *server;
		signal(SIGINT, ctrlC);

		switch(argc){
				case 1:
						width = 640;
						height = 480;
						strcat(camera_device,"0");
						break;
				case 3:
						width = atoi(argv[1]);
						height = atoi(argv[2]);
						strcat(camera_device,"0");
						break;
				case 4:
						width = atoi(argv[1]);
						height = atoi(argv[2]);
						strcat(camera_device,argv[3]);
						break;
				default:
						printf("Usage: ./CVclient <video width> <video hight> <camera device>\n");
						return -1; 
		}
		// portno = atoi(argv[2]);
		// sockfd = socket(AF_INET, SOCK_STREAM, 0);

		/* open socket and connect to server */
		// if (sockfd < 0) 
		//     printf("ERROR opening socket");
		// server = gethostbyname(argv[1]);
		// if (server == NULL) {
		//     fprintf(stderr,"ERROR, no such host\n");
		//     exit(0);
		// }
		// bzero((char *) &serv_addr, sizeof(serv_addr));
		// serv_addr.sin_family = AF_INET;
		// bcopy((char *)server->h_addr, 
		//      (char *)&serv_addr.sin_addr.s_addr,
		//      server->h_length);
		// serv_addr.sin_port = htons(portno);
		// if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		//     printf("ERROR connecting");

		/* open camera and initialize */
		fd = open(camera_device, O_RDWR| O_NONBLOCK, 0);
		if (fd == -1)
		{
				perror("Opening video device");
				return 1;
		}
		camerafd = fd;
		if(print_caps(fd))
				return 1;

		if(init_mmap(fd))
				return 1;

		/*send image */

		capture_and_send_image(fd);


		close(fd);
		return 0;
}
