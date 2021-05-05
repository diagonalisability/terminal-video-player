extern "C" {
	#include<libavformat/avformat.h>
	#include<libavutil/avutil.h>
	#include<libavcodec/avcodec.h>
	#include<libavutil/imgutils.h> // for av_image_alloc
	#include<libswscale/swscale.h>
}
#include"generic.cpp"
#include<csignal>
void avWrapError(S err) {
	if(err==0)
		return;
	error(av_err2str(err));
}
struct Frame {
	U width,height;
	U8 *o;
	Frame(Frame const&)= delete;
	Frame(U width,U height):width{width},height{height},o{new U8[height*width]()} {}
	~Frame() {
		delete[] o;
	}
};
// change this function if you think you can improve the contrast
void printFrame(Frame const &frame) {
	char const chars[]= {' ','.',',',':','%','#'};
	for(U i=0,k=0;i<frame.height;++i) {
		for(U j=0;j<frame.width;++j,++k)
			out(chars[frame.o[k]*(sizeof chars)/256]);
		out(nl);
	}
}
std::atomic<bool> isRunning= 1;
void handleInterrupt(S signal) {
	if(signal != SIGINT)
		error("interrupt handler called for wrong signal");
	isRunning= 0;
	out("stopping..." nl);
}
tp<tn Func> struct ScopeGuard {
	Func func;
	ScopeGuard(Func func):func{func} {}
	~ScopeGuard() { func(); }
};
void playbackToTerminal(std::string const &filename, std::atomic<bool> const &isRunning) {
	AVFormatContext *fc= nullptr; // format context
//	AVFrameImageAdapter outFrame;
	avWrapError(avformat_open_input(
		&fc, // libav allocates the AVFormatContext
		filename.c_str(), // url
		nullptr, // format (autodetected)
		nullptr // dictionary of AVFormatContext options (empty)
	));
	ScopeGuard g0_{[&]{ avformat_close_input(&fc); }};
	out("opened input");
	WATCH(fc->duration);
	WATCH(fc->streams);
	S streamI= 0;
	for(;; ++streamI) {
		if(streamI == fc->nb_streams)
			error("no video stream in the input");
		if(fc->streams[streamI]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			break;
	}
	// find decoder for the stream
	AVCodec *const codec= avcodec_find_decoder(fc->streams[streamI]->codecpar->codec_id);
	if(!codec)
		error("finding codec");
	WATCH(codec);
	// allocate decoder context
	AVCodecContext *dc= avcodec_alloc_context3(codec);
	ScopeGuard g1_{[&]{ avcodec_free_context(&dc); }};
	if(!dc)
		error("allocating context");
	WATCH(dc);
	// set decoder parameters according to the stream
	avWrapError(avcodec_parameters_to_context(dc,fc->streams[streamI]->codecpar));
	{
		AVDictionary *opts= nullptr;
		av_dict_set(&opts,"refcounted_frames",0,0);
		avWrapError(avcodec_open2(dc,codec,nullptr));
	}
	av_dump_format(fc,0,filename.c_str(),0);
	out("allocating buffer" nl);
	WATCH(dc->width);
	WATCH(dc->height);
	WATCH(dc->pix_fmt);
	AVPacket pk;
	av_init_packet(&pk);
	out("starting decode loop..." nl);
	U i=0;
	auto const startTime= std::chrono::steady_clock::now();
	AVFrame *frame= av_frame_alloc();
	for(;isRunning;++i) {
		Frame output{160,40};
		av_read_frame(fc,&pk);
		if(pk.stream_index != 0) { // stream 0 is the video
			av_packet_unref(&pk);
			continue;
		}
		avcodec_send_packet(dc,&pk);
		{
			S const err= avcodec_receive_frame(dc,frame);
			av_packet_unref(&pk);
			if(err == AVERROR(EAGAIN))
				continue; // need more packets before a full frame can be decoded
			if(err == AVERROR_EOF)
				break;
			if(err) {
				avWrapError(err);
				std::exit(1);
			}
		}
		auto const now= std::chrono::steady_clock::now();
		auto const
			cmp0= now-startTime,
			cmp1= cmp0-std::chrono::milliseconds(500);
		AVRational const &time_base= fc->streams[streamI]->time_base;
		// if half a second behind presentation time, drop the frame
		// (idk if this is a reasonable amount of time)
		if((S64)frame->pts * time_base.num * decltype(cmp1)::period::den < (S64)cmp1.count() * decltype(cmp1)::period::num * time_base.den) {
//			out("too far behind, skipping frame..." nl);
			continue;
		}
		// wait until it's time to display the frame
		std::this_thread::sleep_until(startTime + std::chrono::steady_clock::duration{
			(U64)frame->pts
			* time_base.num
			* std::chrono::steady_clock::period::den
			/ time_base.den
			/ std::chrono::steady_clock::period::num
		});
		// use this to slow down the framerate in case the terminal is lagging
		//std::this_thread::sleep_for(std::chrono::milliseconds(50));
		SwsContext *const sc= sws_getContext(
			frame->width,frame->height, // input dimensions
			AVPixelFormat(frame->format), // src pixel format
			output.width, output.height, // output dimensions
			AV_PIX_FMT_GRAY8, //AV_PIX_FMT_BGRA, // dest pixel format
			SWS_BICUBIC, //SWS_POINT, // rescaling algorithm
			nullptr,nullptr,nullptr // srcFilter, destFilter, resize param
		);
		if(!sc)
			error("creating resizing context");
		sws_scale(
			sc,
			frame->data,frame->linesize,
			0,frame->height,
			(U8 *[]){output.o},(S const[]){static_cast<S>(output.width)}
		);
		sws_freeContext(sc);
		av_frame_unref(frame);
		out(nl); // blank line to separate frames
		printFrame(output);
	}
	av_frame_free(&frame);
}
S main(S argc, char const **argv, char const **envp) {
	if(std::signal(SIGINT,handleInterrupt) == SIG_ERR)
		out("couldn't register interrupt handler, continuing anyway" nl);
	out("it compiles!" nl);
	if(argc != 2)
		error("usage: <executable> <file to play>");
	playbackToTerminal(argv[1],isRunning);
}
