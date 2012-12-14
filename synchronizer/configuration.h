/*****************************************************************************
 *   Simon Rieche       simon.rieche@uni-tuebingen.de                        *
 *   Junior Research Group on Protocol-Engineering and Distributed Systems   *
 *   http://ps.ri.uni-tuebingen.de                                           *
 *   Wilhelm Schickard Institute of Computer Science                         *
 *   University of Tuebingen                                                 *
 *                                                                           *
 *   based on: http://www.adp-gmbh.ch/cpp/config_file.html                   *
 *************************	****************************************************/

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <string>
#include <map>

std::string trim(std::string const& source, char const* delims=" \n\r\t");

class Configuration {
  std::map<std::string,std::string> content_;

public:
  Configuration(std::string const& configFile);
  ~Configuration();

  bool        isPresent  (std::string const& section, std::string const& entry);
  std::string getAsString(std::string const& section, std::string const& entry, std::string const& def="");
  int         getAsInt   (std::string const& section, std::string const& entry, int const def=0);
  bool        getAsBool  (std::string const& section, std::string const& entry, bool const def=false);
  double      getAsDouble(std::string const& section, std::string const& entry, double const def=0);
};

#endif
