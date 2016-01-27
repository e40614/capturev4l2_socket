#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
// #include <iostream>
// #include <sys/wait.h>
// using namespace cv;
// using namespace std;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, width, height, childpid;
     socklen_t clilen;
     char messagerec[30] = "";
     char *mesptr = messagerec;
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     char killbuf[20];


    // startWindowThread();
     if (argc < 2) {
         printf("Usage: ./CVserver <server port>\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     for(;;){
         clilen = sizeof(cli_addr);
         newsockfd = accept(sockfd, 
                     (struct sockaddr *) &cli_addr, 
                     &clilen);
        if (newsockfd < 0) 
              error("ERROR on accept");
        if((childpid = fork()) < 0)
            error("server:fork error");
        else if(childpid == 0){//child
            uint32_t framesize;
            uint32_t buffersize;

                if(read(newsockfd, (char*) &width, 4) == 0){
                printf("client closed\n");
                    printf("exit %d\n",(int)getpid());
                    sprintf(killbuf,"kill %d",(int)getpid());
                    n = system((const char*)killbuf);
                  exit(0);
                }
                if(read(newsockfd, (char*) &height, 4)==0){
                printf("client closed\n");
                    printf("exit %d\n",(int)getpid());
                    sprintf(killbuf,"kill %d",(int)getpid());
                    n = system((const char*)killbuf);
                    exit(0);
                }
                if(read(newsockfd, (char*) &buffersize, 4) == 0){
                printf("client closed\n");
                    printf("exit %d\n",(int)getpid());
                    sprintf(killbuf,"kill %d",(int)getpid());
                    n = system((const char*)killbuf);
                    exit(0);
                }

                char buffer[buffersize];
                char *bufferptr;

         //Receive data here
                int n;
            while(1){

                if(read(newsockfd, (char*) &framesize, 4) == 0){
                  printf("client closed\n");
                    printf("exit %d\n",(int)getpid());
                    sprintf(killbuf,"kill %d",(int)getpid());
                    n = system((const char*)killbuf);
                    exit(0);
                }
                bufferptr = buffer;
                while( (n = read(newsockfd, bufferptr, framesize)) != framesize){
                  if(n == 0){
                    printf("client closed\n");
                    printf("exit %d\n",(int)getpid());
                    sprintf(killbuf,"kill %d",(int)getpid());
                    n = system((const char*)killbuf);
                    exit(0);
                  }
                  bufferptr += n;
                  framesize -= n;
                }
                // memset(&buffer[framesize],0,buffersize-framesize-1);
                IplImage* frame;
                CvMat cvmat = cvMat(height, width, CV_8UC3, (void*)buffer);
                frame = cvDecodeImage(&cvmat, 1);
                cvNamedWindow("window",CV_WINDOW_AUTOSIZE);
                cvShowImage("window", frame);
                cvReleaseImage(&frame);
                cvWaitKey(10);

             }
            close(newsockfd);
            close(sockfd);
            exit(0); 
        }
        //parent
        close(newsockfd);


    
    }
}