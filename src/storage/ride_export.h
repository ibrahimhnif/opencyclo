#pragma once
#include <string>
#include <ctype.h>
inline bool safeRideName(const std::string& name) {
  if(name.size()<5 || name.size()>58 || name.substr(name.size()-4)!=".gpx")return false;
  for(char c:name)if(!isalnum(static_cast<unsigned char>(c)) && c!='_' && c!='.' && c!='-')return false;
  return name.find("..") == std::string::npos;
}
