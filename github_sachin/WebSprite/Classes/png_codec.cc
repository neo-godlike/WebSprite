//
//  png_codec.cpp
//  cocos2d_tests
//
//  Created by sachin on 5/10/14.
//
//

#include "png_codec.h"


namespace util {
namespace {

// Converts BGRA->RGBA and RGBA->BGRA.
void ConvertBetweenBGRAandRGBA(const unsigned char* input, int pixel_width,
                               unsigned char* output, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &input[x * 4];
    unsigned char* pixel_out = &output[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = pixel_in[3];
  }
}

void ConvertRGBAtoRGB(const unsigned char* rgba, int pixel_width,
                      unsigned char* rgb, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgba[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[0];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[2];
  }
}
}  // namespace

// Decoder --------------------------------------------------------------------
//
// This code is based on WebKit libpng interface (PNGImageDecoder), which is
// in turn based on the Mozilla png decoder.

namespace {

// Gamma constants: We assume we're on Windows which uses a gamma of 2.2.
const double kMaxGamma = 21474.83;  // Maximum gamma accepted by png library.
const double kDefaultGamma = 2.2;
const double kInverseGamma = 1.0 / kDefaultGamma;


// Called when the png header has been read. This code is based on the WebKit
// PNGImageDecoder
void DecodeInfoCallback(png_struct* png_ptr, png_info* info_ptr) {
  PNGCodec::PngImageReader* png_reader = static_cast<PNGCodec::PngImageReader*>(
                                                         png_get_progressive_ptr(png_ptr));
  
  int bit_depth, color_type, interlace_type, compression_type;
  int filter_type;
  png_uint_32 w, h;
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);
  
  // Bounds check. When the image is unreasonably big, we'll error out and
  // end up back at the setjmp call when we set up decoding.  "Unreasonably big"
  // means "big enough that w * h * 32bpp might overflow an int"; we choose this
  // threshold to match WebKit and because a number of places in code assume
  // that an image's size (in bytes) fits in a (signed) int.
  unsigned long long total_size =
  static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h);
  if (total_size > ((1 << 29) - 1))
    longjmp(png_jmpbuf(png_ptr), 1);
  png_reader->width_ = static_cast<int>(w);
  png_reader->height_ = static_cast<int>(h);
  
  // The following png_set_* calls have to be done in the order dictated by
  // the libpng docs. Please take care if you have to move any of them. This
  // is also why certain things are done outside of the switch, even though
  // they look like they belong there.
	png_reader->png_color_type_ = color_type;
  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand(png_ptr);
  
  // The '!= 0' is for silencing a Windows compiler warning.
  bool input_has_alpha = ((color_type & PNG_COLOR_MASK_ALPHA) != 0);
  
  // Transparency for paletted images.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_expand(png_ptr);
    input_has_alpha = true;
  }
  
  // Convert 16-bit to 8-bit.
  if (bit_depth == 16)
    png_set_strip_16(png_ptr);
  
  // Pick our row format converter necessary for this data.
  if (!input_has_alpha) {
    png_reader->output_channels_ = 4;
    png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
  } else {
    png_reader->output_channels_ = 4;
  }
	/*switch (color_type)
	{
	case PNG_COLOR_TYPE_GRAY:
		png_reader->output_channels_ = 1;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		png_reader->output_channels_ = 2;
		break;
	case PNG_COLOR_TYPE_RGB:
		png_reader->output_channels_ = 3;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		png_reader->output_channels_ = 4;
		break;
	default:
		break;
	}*/
  // Expand grayscale to RGB.
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);
  
  // Deal with gamma and keep it under our control.
  double gamma;
  if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
    if (gamma <= 0.0 || gamma > kMaxGamma) {
      gamma = kInverseGamma;
      png_set_gAMA(png_ptr, info_ptr, gamma);
    }
    png_set_gamma(png_ptr, kDefaultGamma, gamma);
  } else {
    png_set_gamma(png_ptr, kDefaultGamma, kInverseGamma);
  }
  
  
  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png_ptr);
  
  png_read_update_info(png_ptr, info_ptr);
  
  png_reader->buffer_size_ = png_reader->width_ * png_reader->output_channels_ * png_reader->height_;
  png_reader->interlace_buffer_ = new png_byte[png_reader->buffer_size_];
  png_reader->decode_state_ = PNGCodec::DecodeState::DECODE_HEADER_DONE;
	png_reader->read_header_complete_callback_(png_reader->custom_ptr);
}

void DecodeRowCallback(png_struct* png_ptr, png_byte* new_row,
                       png_uint_32 row_num, int pass) {
  if (!new_row)
    return;  // Interlaced image; row didn't change this pass.
  
  PNGCodec::PngImageReader* png_reader = static_cast<PNGCodec::PngImageReader*>(
                                                                                png_get_progressive_ptr(png_ptr));
  png_reader->decode_state_ = PNGCodec::DecodeState::DECODING_DATA;
  if (static_cast<int>(row_num) > png_reader->height_) {
    //NOTREACHED() << "Invalid row";
    return;
  }
  if (new_row != NULL) {
    unsigned char* base = NULL;
    base = png_reader->interlace_buffer_;
    png_reader->buffer_size_ = png_reader->width_ * png_reader->output_channels_ * (row_num + 1);
    unsigned char* dest = &base[png_reader->width_ * png_reader->output_channels_ * row_num];
    png_progressive_combine_row(png_ptr, dest, new_row);
    png_reader->read_row_complete_callback_(png_reader->custom_ptr, pass);
  }
}

void DecodeEndCallback(png_struct* png_ptr, png_info* info) {
  PNGCodec::PngImageReader* png_reader = static_cast<PNGCodec::PngImageReader*>(
                                                                                png_get_progressive_ptr(png_ptr));
  
  // Mark the image as complete, this will tell the Decode function that we
  // have successfully found the end of the data.
  png_reader->decode_state_ = PNGCodec::DecodeState::DECODE_DONE;
  png_reader->read_all_complete_callback_(png_reader->custom_ptr);
}

// Automatically destroys the given read structs on destruction to make
// cleanup and error handling code cleaner.
class PngReadStructDestroyer {
public:
  PngReadStructDestroyer(png_struct** ps, png_info** pi) : ps_(ps), pi_(pi) {
  }
  ~PngReadStructDestroyer() {
    png_destroy_read_struct(ps_, pi_, NULL);
  }
private:
  png_struct** ps_;
  png_info** pi_;
  DISALLOW_COPY_AND_ASSIGN(PngReadStructDestroyer);
};


  
// Automatically destroys the given write structs on destruction to make
// cleanup and error handling code cleaner.
class PngWriteStructDestroyer {
public:
  explicit PngWriteStructDestroyer(png_struct** ps) : ps_(ps), pi_(0) {
  }
  ~PngWriteStructDestroyer() {
    png_destroy_write_struct(ps_, pi_);
  }
  void SetInfoStruct(png_info** pi) {
    pi_ = pi;
  }
private:
  png_struct** ps_;
  png_info** pi_;
  DISALLOW_COPY_AND_ASSIGN(PngWriteStructDestroyer);
};

bool BuildPNGStruct(const unsigned char* input, size_t input_size,
                    png_struct** png_ptr, png_info** info_ptr) {
  if (input_size < 8)
    return false;  // Input data too small to be a png
  
  // Have libpng check the signature, it likes the first 8 bytes.
  if (png_sig_cmp(const_cast<unsigned char*>(input), 0, 8) != 0)
    return false;
  
  *png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!*png_ptr)
    return false;
  
  *info_ptr = png_create_info_struct(*png_ptr);
  if (!*info_ptr) {
    png_destroy_read_struct(png_ptr, NULL, NULL);
    return false;
  }
  
  return true;
}

// Libpng user error and warning functions which allows us to print libpng
// errors and warnings using Chrome's logging facilities instead of stderr.

void LogLibPNGDecodeError(png_structp png_ptr, png_const_charp error_msg) {
  //DLOG(ERROR) << "libpng decode error: " << error_msg;
  longjmp(png_jmpbuf(png_ptr), 1);
}

void LogLibPNGDecodeWarning(png_structp png_ptr, png_const_charp warning_msg) {
  //DLOG(ERROR) << "libpng decode warning: " << warning_msg;
}

void LogLibPNGEncodeError(png_structp png_ptr, png_const_charp error_msg) {
  //DLOG(ERROR) << "libpng encode error: " << error_msg;
  longjmp(png_jmpbuf(png_ptr), 1);
}

void LogLibPNGEncodeWarning(png_structp png_ptr, png_const_charp warning_msg) {
  //DLOG(ERROR) << "libpng encode warning: " << warning_msg;
}

}  // namespace anouymous

PNGCodec::PNGCodec() {

}

bool PNGCodec::PrepareDecode() {
  png_reader_.png_struct_ptr_= png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_reader_.png_struct_ptr_)
    return false;
  
  png_reader_.png_info_ptr_ = png_create_info_struct(png_reader_.png_struct_ptr_);
  if (!png_reader_.png_info_ptr_) {
    png_destroy_read_struct(&png_reader_.png_struct_ptr_, NULL, NULL);
    return false;
  }

  if (setjmp(png_jmpbuf(png_reader_.png_struct_ptr_))) {
    png_destroy_read_struct(&png_reader_.png_struct_ptr_, &png_reader_.png_info_ptr_, (png_infopp)NULL);
    return false;
  }
  
  png_set_error_fn(png_reader_.png_struct_ptr_, NULL, LogLibPNGDecodeError, LogLibPNGDecodeWarning);
  
  png_set_progressive_read_fn(png_reader_.png_struct_ptr_, &png_reader_, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_reader_.decode_state_ = PNGCodec::DecodeState::DECODE_READY;
  return true;
}
  
bool PNGCodec::Decoding(unsigned char *input,size_t input_size) {
  if (setjmp(png_jmpbuf(png_reader_.png_struct_ptr_))) {
    png_destroy_read_struct(&png_reader_.png_struct_ptr_, &png_reader_.png_info_ptr_, (png_infopp)NULL);
    return false;
  }
  png_process_data(png_reader_.png_struct_ptr_, png_reader_.png_info_ptr_, input, input_size);
  return true;
}
  
PNGCodec::PngImageReader::PngImageReader()
    : png_struct_ptr_(NULL)
    , png_info_ptr_(NULL)
    , interlace_buffer_(NULL)
    , decode_state_(UNBEGIN)
    , width_(0)
    , height_(0)
    , buffer_size_(0)
    , output_channels_(0)
    , png_color_type_(0){
    
}

PNGCodec::PngImageReader::~PngImageReader() {
  if (png_struct_ptr_ && png_info_ptr_) {
    // This will zero the pointers.
    png_destroy_read_struct(&png_struct_ptr_, &png_info_ptr_, 0);
  }
  if (interlace_buffer_) {
    delete[] interlace_buffer_;
    interlace_buffer_ = 0;
  }
}
// static
bool PNGCodec::Decode(const unsigned char* input, size_t input_size,
                    ColorFormat format, std::vector<unsigned char>* output,
                    int* w, int* h) {
  PNGCodec::PngImageReader png_reader;
  if (!BuildPNGStruct(input, input_size, &png_reader.png_struct_ptr_, &png_reader.png_info_ptr_))
    return false;

  PngReadStructDestroyer destroyer(&png_reader.png_struct_ptr_, &png_reader.png_info_ptr_);
  if (setjmp(png_jmpbuf(png_reader.png_struct_ptr_))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }



  png_set_error_fn(png_reader.png_struct_ptr_, NULL, LogLibPNGDecodeError, LogLibPNGDecodeWarning);
  png_set_progressive_read_fn(png_reader.png_struct_ptr_, &png_reader, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_process_data(png_reader.png_struct_ptr_,
                   png_reader.png_info_ptr_,
                   const_cast<unsigned char*>(input),
                   input_size);

  if (!(png_reader.decode_state_ == DECODE_DONE)) {
    // Fed it all the data but the library didn't think we got all the data, so
    // this file must be truncated.
    output->clear();
    return false;
  }

  *w = png_reader.width_;
  *h = png_reader.height_;
  return true;
}

// Encoder --------------------------------------------------------------------
//
// This section of the code is based on nsPNGEncoder.cpp in Mozilla
// (Copyright 2005 Google Inc.)

namespace {

// Passed around as the io_ptr in the png structs so our callbacks know where
// to write data.
struct PngEncoderState {
  explicit PngEncoderState(std::vector<unsigned char>* o) : out(o) {}
  std::vector<unsigned char>* out;
};

// Called by libpng to flush its internal buffer to ours.
void EncoderWriteCallback(png_structp png, png_bytep data, png_size_t size) {
  PngEncoderState* state = static_cast<PngEncoderState*>(png_get_io_ptr(png));
  //DCHECK(state->out);
  
  size_t old_size = state->out->size();
  state->out->resize(old_size + size);
  memcpy(&(*state->out)[old_size], data, size);
}

void FakeFlushCallback(png_structp png) {
  // We don't need to perform any flushing since we aren't doing real IO, but
  // we're required to provide this function by libpng.
}

void ConvertBGRAtoRGB(const unsigned char* bgra, int pixel_width,
                      unsigned char* rgb, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &bgra[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
  }
}

// The type of functions usable for converting between pixel formats.
typedef void (*FormatConverter)(const unsigned char* in, int w,
unsigned char* out, bool* is_opaque);

// libpng uses a wacky setjmp-based API, which makes the compiler nervous.
// We constrain all of the calls we make to libpng where the setjmp() is in
// place to this function.
// Returns true on success.
bool DoLibpngWrite(png_struct* png_ptr, png_info* info_ptr,
                   PngEncoderState* state,
                   int width, int height, int row_byte_width,
                   const unsigned char* input, int compression_level,
                   int png_output_color_type, int output_color_components,
                   FormatConverter converter,
                   const std::vector<PNGCodec::Comment>& comments) {
  unsigned char* row_buffer = NULL;
  
  // Make sure to not declare any locals here -- locals in the presence
  // of setjmp() in C++ code makes gcc complain.
  
  if (setjmp(png_jmpbuf(png_ptr))) {
    delete[] row_buffer;
    return false;
  }
  
  png_set_compression_level(png_ptr, compression_level);
  
  // Set our callback for libpng to give us the data.
  png_set_write_fn(png_ptr, state, EncoderWriteCallback, FakeFlushCallback);
  png_set_error_fn(png_ptr, NULL, LogLibPNGEncodeError, LogLibPNGEncodeWarning);
  
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, png_output_color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  
  png_write_info(png_ptr, info_ptr);
  
  if (!converter) {
    // No conversion needed, give the data directly to libpng.
    for (int y = 0; y < height; y ++) {
      png_write_row(png_ptr,
                    const_cast<unsigned char*>(&input[y * row_byte_width]));
    }
  } else {
    // Needs conversion using a separate buffer.
    row_buffer = new unsigned char[width * output_color_components];
    for (int y = 0; y < height; y ++) {
      converter(&input[y * row_byte_width], width, row_buffer, NULL);
      png_write_row(png_ptr, row_buffer);
    }
    delete[] row_buffer;
  }
  
  png_write_end(png_ptr, info_ptr);
  return true;
}

bool EncodeWithCompressionLevel(const unsigned char* input,
                                PNGCodec::ColorFormat format,
                                const Size& size,
                                int row_byte_width,
                                bool discard_transparency,
                                const std::vector<PNGCodec::Comment>& comments,
                                int compression_level,
                                std::vector<unsigned char>* output) {
  // Run to convert an input row into the output row format, NULL means no
  // conversion is necessary.
  FormatConverter converter = NULL;
  
  int input_color_components, output_color_components;
  int png_output_color_type;
  switch (format) {
    case PNGCodec::FORMAT_RGB:
      input_color_components = 3;
      output_color_components = 3;
      png_output_color_type = PNG_COLOR_TYPE_RGB;
      break;
      
    case PNGCodec::FORMAT_RGBA:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertRGBAtoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = NULL;
      }
      break;
      
    case PNGCodec::FORMAT_BGRA:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertBGRAtoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = ConvertBetweenBGRAandRGBA;
      }
      break;
    default:
      //NOTREACHED() << "Unknown pixel format";
      return false;
  }
  
  // Row stride should be at least as long as the length of the data.
  //DCHECK(input_color_components * size.width() <= row_byte_width);
  
  png_struct* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                NULL, NULL, NULL);
  if (!png_ptr)
    return false;
  PngWriteStructDestroyer destroyer(&png_ptr);
  png_info* info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    return false;
  destroyer.SetInfoStruct(&info_ptr);
  
  output->clear();
  
  PngEncoderState state(output);
  bool success = DoLibpngWrite(png_ptr, info_ptr, &state,
                               size.width(), size.height(), row_byte_width,
                               input, compression_level, png_output_color_type,
                               output_color_components, converter, comments);
  
  return success;
}

}  // namespace

// static
bool PNGCodec::Encode(const unsigned char* input,
                    ColorFormat format,
                    const Size& size,
                    int row_byte_width,
                    bool discard_transparency,
                    const std::vector<Comment>& comments,
                    std::vector<unsigned char>* output) {
#define Z_DEFAULT_COMPRESSION  (-1)
  return EncodeWithCompressionLevel(input,
                                  format,
                                  size,
                                  row_byte_width,
                                  discard_transparency,
                                  comments,
                                  Z_DEFAULT_COMPRESSION,
                                  output);
}


PNGCodec::Comment::Comment(const std::string& k, const std::string& t)
: key(k), text(t) {
}

PNGCodec::Comment::~Comment() {
}


} // namespace util