//
//  png_codec.h
//  cocos2d_tests
//
//  Created by sachin on 5/10/14.
//
//

#ifndef UTIL_PNG_CODEC_H_
#define UTIL_PNG_CODEC_H_

#include <string>
#include <vector>

#include "png.h"


namespace util {
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
TypeName(const TypeName&);               \
void operator=(const TypeName&)


class Size {
 public:
  int width() const {return width_;}
  int height() const {return height_;}
 private:
  int width_;
  int height_;
};
// Interface for encoding and decoding PNG data. This is a wrapper around
// libpng, which has an inconvenient interface for callers. This is currently
// designed for use in tests only (where we control the files), so the handling
// isn't as robust as would be required for a browser (see Decode() for more).
// WebKit has its own more complicated PNG decoder which handles, among other
// things, partially downloaded data.
class  PNGCodec {
 public:
  PNGCodec();
  ~PNGCodec() {}
  enum ColorFormat {
    // 3 bytes per pixel (packed), in RGB order regardless of endianness.
    // This is the native JPEG format.
    FORMAT_RGB,
    
    // 4 bytes per pixel, in RGBA order in memory regardless of endianness.
    FORMAT_RGBA,
    
    // 4 bytes per pixel, in BGRA order in memory regardless of endianness.
    // This is the default Windows DIB order.
    FORMAT_BGRA,
    
  };
  enum DecodeState {
    UNBEGIN,
    DECODE_READY,
    DECODE_HEADER_DONE,
    DECODING_DATA,
    DECODE_DONE
  };
  // Represents a comment in the tEXt ancillary chunk of the png.
  struct  Comment {
    Comment(const std::string& k, const std::string& t);
    ~Comment();
    
    std::string key;
    std::string text;
  };

	typedef void(*ReadHeaderCompleteCallBack)(void*);
  typedef void (*ReadRowCompleteCallBack)(void*, int);
  typedef void (*ReadAllCompleteCallBack)(void*);

  class PngImageReader {
  public:
    PngImageReader();
    ~PngImageReader();
    png_structp png_struct_ptr_;
    png_infop png_info_ptr_;
    // Size of the image, set in the info callback.
    int width_;
    int height_;
    
    int output_channels_;
    int png_color_type_;
    
    // Set to true when we've found the end of the data.
    PNGCodec::DecodeState decode_state_;
    png_bytep interlace_buffer_;
    size_t buffer_size_;
		ReadHeaderCompleteCallBack read_header_complete_callback_;
    ReadRowCompleteCallBack read_row_complete_callback_;
    ReadAllCompleteCallBack read_all_complete_callback_;
    void* custom_ptr;
  };

  
  bool PrepareDecode();
	void SetReadCallBack(void* ptr, ReadHeaderCompleteCallBack header_callback, ReadRowCompleteCallBack row_callback, ReadAllCompleteCallBack end_callback) {
		png_reader_.read_header_complete_callback_ = header_callback;
		png_reader_.read_row_complete_callback_ = row_callback;
    png_reader_.read_all_complete_callback_ = end_callback;
    png_reader_.custom_ptr = ptr;
  }
  
  bool Decoding(unsigned char* input, size_t input_size);
  DecodeState  decode_state() {return png_reader_.decode_state_;}
  
  int png_width() {return png_reader_.width_;}
  int png_height() {return png_reader_.height_;}
	int png_output_channels() { return png_reader_.output_channels_; }
  int png_color_type() {return png_reader_.png_color_type_;}
  size_t png_data_size() {return png_reader_.buffer_size_;}
  png_bytep png_data_buffer() {return png_reader_.interlace_buffer_;}
  
 private:
  // Encodes the given raw 'input' data, with each pixel being represented as
  // given in 'format'. The encoded PNG data will be written into the supplied
  // vector and true will be returned on success. On failure (false), the
  // contents of the output buffer are undefined.
  //
  // When writing alpha values, the input colors are assumed to be post
  // multiplied.
  //
  // size: dimensions of the image
  // row_byte_width: the width in bytes of each row. This may be greater than
  //   w * bytes_per_pixel if there is extra padding at the end of each row
  //   (often, each row is padded to the next machine word).
  // discard_transparency: when true, and when the input data format includes
  //   alpha values, these alpha values will be discarded and only RGB will be
  //   written to the resulting file. Otherwise, alpha values in the input
  //   will be preserved.
  // comments: comments to be written in the png's metadata.
  static bool Encode(const unsigned char* input,
                     ColorFormat format,
                     const Size& size,
                     int row_byte_width,
                     bool discard_transparency,
                     const std::vector<Comment>& comments,
                     std::vector<unsigned char>* output);
  

  // Decodes the PNG data contained in input of length input_size. The
  // decoded data will be placed in *output with the dimensions in *w and *h
  // on success (returns true). This data will be written in the 'format'
  // format. On failure, the values of these output variables are undefined.
  //
  // This function may not support all PNG types, and it hasn't been tested
  // with a large number of images, so assume a new format may not work. It's
  // really designed to be able to read in something written by Encode() above.
  static bool Decode(const unsigned char* input, size_t input_size,
                     ColorFormat format, std::vector<unsigned char>* output,
                     int* w, int* h);
 private:
  PngImageReader png_reader_;
 private:
  DISALLOW_COPY_AND_ASSIGN(PNGCodec);
};

} // namespce util
#endif // UTIL_PNG_CODEC_H_
