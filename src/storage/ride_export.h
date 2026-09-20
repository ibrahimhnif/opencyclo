#pragma once
#include <string>
#include <string.h>
#include <ctype.h>
inline bool safeExportName(const std::string& name, const char* ext) {
  const size_t extLen=strlen(ext);
  if(name.size()<extLen+1 || name.size()>58 || name.substr(name.size()-extLen)!=ext)return false;
  for(char c:name)if(!isalnum(static_cast<unsigned char>(c)) && c!='_' && c!='.' && c!='-')return false;
  return name.find("..") == std::string::npos;
}
inline bool safeRideName(const std::string& name) { return safeExportName(name, ".gpx"); }
inline bool safeScreenshotName(const std::string& name) { return safeExportName(name, ".bmp"); }
