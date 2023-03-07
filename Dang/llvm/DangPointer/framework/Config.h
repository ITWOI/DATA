#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>

class Config
{
	std::unordered_map<std::string, std::unordered_map<std::string,std::string>> options;
	std::pair<std::string, std::string> parseOptionLine(std::string optionLine);
	std::string trim(std::string s);
public:
	Config(std::string configFile);
	std::unordered_map<std::string, std::string> getOptionBlock(std::string blockName);
	friend std::ostream & operator <<(std::ostream &os, Config &c);
	std::unordered_map<std::string, std::unordered_map<std::string,std::string>> getAllOptionBlocks();
    void add(std::string name, std::unordered_map<std::string, std::string> content) {
        options[name] = content;
    }
};


#endif
