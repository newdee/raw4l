#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define u8 unsigned char
#define SHOW(...) printf(__VA_ARGS__);
#define DBG(fmt,args...) SHOW("%s:%d, \n"fmt, __FUNCTION__,__LINE__,##args);

#define ASSERT(e) {if (!e) SHOW("error on %s:%d ...\n",__FUNCTION__,__LINE__);}

#define VDEV "/dev/video0"
#define IMG_WIDTH 3840 //set images width and height
#define IMG_HEIGHT 2160
#define IMG_SIZE (IMG_WIDTH*IMG_HEIGHT*2)
#define BUF_CNT 3 //request 5 buffers
#define PIX_FMT V4L2_PIX_FMT_SRGGB10


struct v4l2_buffer v_buf[BUF_CNT];
u8 *v_buf_p[BUF_CNT];//video buffer ptr
u8 buf[IMG_SIZE];

int main()
{

    int cam_fd=-1;
    //camera open
    cam_fd= open(VDEV,O_RDWR);//open camera
    if(cam_fd <0) return -1;

    //camera use
    int ret;
    int input = 0;
    ret = ioctl(cam_fd,VIDIOC_S_INPUT,&input);
    ASSERT(ret == 0);

    //camera init
    int i; //for recycle
    struct v4l2_format format;

    //capture format settings
    memset(&format,0,sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; //frame's type to capture;

    format.fmt.pix.pixelformat = PIX_FMT; //CAPTURE  SETTING!! 10bit raw FORMAT!!
    format.fmt.pix.width = IMG_WIDTH;
    format.fmt.pix.height = IMG_HEIGHT;
    ret = ioctl(cam_fd,VIDIOC_TRY_FMT,&format);//try to set the format
    if(ret !=0)
     {
         DBG("ioctl(VIDIOC_TRY_FMT) failed %d(%s)\n",errno,strerror(errno));
         return ret;
     }
 
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(cam_fd,VIDIOC_S_FMT,&format); // set the format
    //ret = ioctl(cam_fd,VIDIOC_G_FMT,&format); // get the format
    if(ret!=0)
    {
        DBG("ioctl(VIDIOC_S_FMT) failed %d(%s)\n",errno, strerror(errno));
        return ret;
    }


//buffer request
    struct v4l2_requestbuffers req;
    req.count = BUF_CNT; //buffer frame  count
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; //buffer frame's format
    req.memory = V4L2_MEMORY_MMAP; //memory map method

    ret = ioctl(cam_fd,VIDIOC_REQBUFS,&req);
    if(ret!=0)
    {
        DBG("ioctl(VIDIOC_REQBUFS) failed %d(%s)\n",errno,strerror(errno));
        return ret;
    }
    DBG("req.count:%d\n",req.count);
    if(req.count < BUF_CNT)
    {
        DBG("request buffer failed\n");
        return ret;
    }


//query buffer
    struct v4l2_buffer buffer;
    memset(&buffer,0,sizeof(buffer));
    buffer.memory=V4L2_MEMORY_MMAP;
    //buffer.type = req.type;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;



//Geting Buffers
    for(i=0;i<req.count;i++)
    {
    buffer.index =1;
    ret = ioctl(cam_fd, VIDIOC_QUERYBUF,&buffer); // get frame buffer address
    if(ret !=0)
    {
        DBG("ioctl(VIDIOC_QUERYBUF) failed %d(%s)\n",errno,strerror(errno));
        return ret;
    }

    DBG("BUFFER LENGTH:%d\t",buffer.length);
    DBG("BUFFER.m.offset:%d\n",buffer.m.offset);
    v_buf_p[i] = (u8*) mmap(NULL,buffer.length,PROT_READ|PROT_WRITE,MAP_SHARED,cam_fd,buffer.m.offset); // memory maping
    if(v_buf_p[i] == MAP_FAILED)
    {
        DBG("mmap() failed %d(%s)\n",errno,strerror(errno));
        return -1;
    }

   // buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index =i;
    ret = ioctl(cam_fd,VIDIOC_QBUF,&buffer);
    if(ret!=0)
    {
        DBG("ioctl(VIDIOC_QBUF) failed %d(%s)\n", errno, strerror(errno));
        return ret;
    }

    }

    int buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret=ioctl(cam_fd,VIDIOC_STREAMON,&buffer_type); //start the stream
    if(ret !=0)
    {
        DBG("ioctl(VIDIOC_STREAMON) failed %d(%s)\n", errno,strerror(errno));
        return ret;
    }
    DBG("camera init finished\n");
    

    int count=0;
    while(count <10)
      {
        struct v4l2_buffer buffer;
        memset(&buffer,0,sizeof(buffer));
        buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory=V4L2_MEMORY_MMAP;
        buffer.index = BUF_CNT;

        ret = ioctl(cam_fd,VIDIOC_DQBUF,&buffer);
        if(ret!=0)
        {
            DBG("ioctl(VIDIOC_DQBUF) failed %d (%s)\n",errno, strerror(errno));
            return ret;
        }
        if(buffer.index <0 || buffer.index >= BUF_CNT)
        {
            DBG("invaild buffer index:%d\n", buffer.index);
            return ret;
        }

        DBG("dequeue done, index:%d\n", buffer.index);
        memcpy(buf,v_buf_p[buffer.index],IMG_SIZE); //copy the data from buffer
        DBG("copy done\n");

        ret = ioctl(cam_fd, VIDIOC_QBUF,&buffer);  //put the buffer on queue
        if(ret !=0)
        {
            DBG("ioctl(VIDIOC_QBUF)failed %d(%s)\n",errno,strerror(errno));
            return ret;
        }
        DBG("enqueue done.\n");


        char tmp[64] = {"---\n"};
        //for(i=0;i<16;i++)
        //    sprintf(&tmp[strlen(tmp)],"%02x\t",buf[i]);
        //SHOW("%s",tmp);

        char filename[32];
        sprintf(filename,"./rawout/%05d.raw",count++);
        printf("writing in %s...\n\n",filename);
        int fd = open(filename,O_WRONLY|O_CREAT,S_IWUSR); //save image data
        if(fd>=0)
        {
            write(fd,buf,IMG_SIZE);
            close(fd);
        }
        else
        {
            SHOW("OPEN() failed %d(%s)",errno,strerror(errno));
        }

    }

    ret=ioctl(cam_fd,VIDIOC_STREAMOFF,&buffer_type); //stop the stream
    if(ret !=0)
    {
        DBG("ioctl(VIDIOC_STREAMOFF) failed %d(%s)\n", errno,strerror(errno));
        return ret;
    }
    DBG("camera STREAMOFF finished\n");
    close(cam_fd); //close camera

    return 0;




}
