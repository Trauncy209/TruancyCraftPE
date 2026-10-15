#ifndef TCPE_JAVA_SKIN_PIXELS_H
#define TCPE_JAVA_SKIN_PIXELS_H
#include <string>
#include <vector>
inline bool decodeJavaRgbaPixels(const std::string& text,int width,int height,std::vector<unsigned char>& pixels){
 pixels.clear();if(width<=0||height<=0||width>8192||height>8192||size_t(width)*size_t(height)>16*1024*1024||text.size()>((size_t(width)*size_t(height)*4+2)/3)*4||text.size()%4)return false;
 const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(size_t i=0;i<text.size();i+=4){unsigned value=0;int pad=0;for(int j=0;j<4;++j){char c=text[i+j];if(c=='='){if(i+4!=text.size()||j<2)return false;++pad;value<<=6;}else{if(pad)return false;size_t n=alphabet.find(c);if(n==std::string::npos)return false;value=(value<<6)|unsigned(n);}}if(pad>2)return false;pixels.push_back(value>>16);if(pad<2)pixels.push_back(value>>8);if(!pad)pixels.push_back(value);}
 return pixels.size()==size_t(width*height*4);
}
inline bool decodeJavaSkinPixels(const std::string& text,int width,int height,std::vector<unsigned char>& pixels){if(width!=64||(height!=32&&height!=64)||text.size()>22000)return false;return decodeJavaRgbaPixels(text,width,height,pixels);}
#endif
