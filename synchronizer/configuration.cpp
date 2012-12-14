/*****************************************************************************
 *   Simon Rieche       simon.rieche@uni-tuebingen.de                        *
 *   Junior Research Group on Protocol-Engineering and Distributed Systems   *
 *   http://ps.ri.uni-tuebingen.de                                           *
 *   Wilhelm Schickard Institute of Computer Science                         *
 *   University of Tuebingen                                                 *
 *                                                                           *
 *   based on: http://www.adp-gmbh.ch/cpp/config_file.html                   *
 *****************************************************************************/

#include "configuration.h"
#include <fstream>
#include <map>
#include <functional>
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <string>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

std::string trim(std::string const& source, char const* delims) {
  std::string result(source);
  std::string::size_type index = result.find_last_not_of(delims);
  if(index != std::string::npos)
    result.erase(++index);

  index = result.find_first_not_of(delims);
  if(index != std::string::npos)
    result.erase(0, index);
  else
    result.erase();
  return result;
}

Configuration::Configuration(std::string const& configFile) {
  std::ifstream file(configFile.c_str());

  std::string line;
  std::string name;
  std::string value;
  std::string inSection;
  int posEqual;
  while (std::getline(file,line)) {

    if (! line.length()) continue;

    if (line[0] == '#') continue;
    if (line[0] == ';') continue;

    if (line[0] == '[') {
      inSection=trim(line.substr(1,line.find(']')-1));
      continue;
    }

    posEqual=line.find('=');
    name  = trim(line.substr(0,posEqual));
    value = trim(line.substr(posEqual+1));

    content_[inSection+'/'+name]=value;
  }
}

Configuration::~Configuration(){}

bool Configuration::isPresent(std::string const& section, std::string const& entry) {
  std::map<std::string,std::string>::const_iterator ci = content_.find(section + '/' + entry);
  return (ci != content_.end());
}

std::string Configuration::getAsString(std::string const& section, std::string const& entry, std::string const& def) {
  std::map<std::string,std::string>::const_iterator ci = content_.find(section + '/' + entry);
  if (ci == content_.end()) 
  {
      //std::cout << "No field"<< std::endl;
      return def;
  }
  return ci->second;
}

int Configuration::getAsInt(std::string const& section, std::string const& entry, int const def){
    int result = def;

	std::string tmp = getAsString(section, entry);
	
	if (0 != tmp.length())
	{
		istringstream value(tmp.c_str());      

		if (!(value >> result))
  		{ 
		  std::cout << "Must be a number: '" << section << "," << entry << "'" << std::endl;
          return def;
		}
	}
    return result;
}

/*
bool Configuration::getAsBool(std::string const& section, std::string const& entry, bool const def){
    bool result = def;
   
    std::string tmp = getAsString(section, entry);
    int (*pf)(int)=tolower; 
    transform(tmp.begin(), tmp.end(), tmp.begin(), pf); 

   	if (0 != tmp.length())
	{
		result = (string("true") == tmp)
                 || (string("yes") == tmp);
	}
    return result;
}
*/

double Configuration::getAsDouble(std::string const& section, std::string const& entry, double const def){
    double result = def;

	std::string tmp = getAsString(section, entry);
	
	if (0 != tmp.length())
	{
		istringstream value(tmp.c_str());      

		if (!(value >> result))
  		{ 
		  std::cout << "Must be a number: '" << section << "," << entry << "'" << std::endl;
          return def;
		}
	}
    return result;
}
