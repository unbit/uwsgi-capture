#include <uwsgi.h>
#include <linux/videodev2.h>

extern struct uwsgi_server uwsgi;

struct capture_config {
	char *v4l_device;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct uwsgi_sharedarea *sa;
} ucapture;

static struct uwsgi_option capture_options[] = {
	{"v4l-capture", required_argument, 0, "start capturing from the specified v4l device", uwsgi_opt_set_str, &ucapture.v4l_device, 0},
	{ NULL, 0, 0, NULL, NULL, NULL, 0},
};

static int captureinit() {
	if (!ucapture.v4l_device) return 0;

	int fd = open(ucapture.v4l_device, O_RDWR|O_NONBLOCK);
	if (fd < 0) {
		uwsgi_error_open(ucapture.v4l_device);
		exit(1);
	}

	ucapture.req.count               = 1;
        ucapture.req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ucapture.req.memory              = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &ucapture.req) < 0) {
		uwsgi_error("ioctl()");
                exit(1);	
	}	

	memset(&ucapture.fmt, 0, sizeof(struct v4l2_format));
	ucapture.fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &ucapture.fmt) < 0) {
		uwsgi_error("ioctl()");
                exit(1);
	}

	uwsgi_log("%s detected width = %d\n", ucapture.v4l_device, ucapture.fmt.fmt.pix.width);
	uwsgi_log("%s detected height = %d\n", ucapture.v4l_device, ucapture.fmt.fmt.pix.height);
	uwsgi_log("%s detected format = %.*s\n", ucapture.v4l_device, 4, &ucapture.fmt.fmt.pix.pixelformat);

	struct v4l2_buffer vbuf;
	memset(&vbuf, 0, sizeof(struct v4l2_buffer));
	vbuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory      = V4L2_MEMORY_MMAP;
        vbuf.index       = 0;

	if (ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0) {
		uwsgi_error("ioctl()");
		exit(1);
	}

	uint64_t area_len = vbuf.length;
	char *area = mmap (NULL, vbuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vbuf.m.offset);

	// create a sharedarea on the mmap()'ed region
	ucapture.sa = uwsgi_sharedarea_init_ptr(area, area_len);
	ucapture.sa->fd = fd;
	ucapture.sa->honour_used = 1;

        memset(&vbuf, 0, sizeof(struct v4l2_buffer));
        vbuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory      = V4L2_MEMORY_MMAP;
        vbuf.index       = 0;

	// enqueue the buf
	if (ioctl(fd, VIDIOC_QBUF, &vbuf) < 0) {
        	uwsgi_error("ioctl()");
                exit(1);
        }

	// start streaming data
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        	uwsgi_error("ioctl()");
                exit(1);
	}

	uwsgi_log("%s started streaming frames to sharedarea %d\n", ucapture.v4l_device, ucapture.sa->id);

	return 0;
}

void captureloop() {

	for(;;) {
		struct pollfd p;
		p.events = POLLIN;
		p.fd = ucapture.sa->fd;
		int ret = poll(&p, 1, -1);
		if (ret < 0){
			uwsgi_error("poll()");
			exit(1);
		}
		//uwsgi_log("ret = %d\n", ret);
		struct v4l2_buffer vbuf;
                memset(&vbuf, 0, sizeof(struct v4l2_buffer));
		vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        	vbuf.memory = V4L2_MEMORY_MMAP;

		// dequeue buf
		uwsgi_wlock(ucapture.sa->lock);
		if (ioctl(ucapture.sa->fd, VIDIOC_DQBUF, &vbuf) < 0) {
			uwsgi_error("ioctl()");
                        exit(1);
		}
		ucapture.sa->updates++;
		ucapture.sa->used = (uint64_t) vbuf.bytesused;
		uwsgi_rwunlock(ucapture.sa->lock);

		// re-enqueue buf
		if (ioctl(ucapture.sa->fd, VIDIOC_QBUF, &vbuf) < 0) {
                        uwsgi_error("ioctl()");
                        exit(1);
                }
	}
}

struct uwsgi_plugin capture_plugin = {
	.name = "capture",
	.options = capture_options,
	.init = captureinit,
};
