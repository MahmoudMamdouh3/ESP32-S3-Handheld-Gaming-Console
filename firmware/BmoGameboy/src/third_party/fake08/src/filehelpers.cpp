#include <string>
#include <vector>

std::string get_file_contents(std::string filename) {
    (void)filename;
    return "";
}

std::vector<char> get_file_as_buffer(std::string filename) {
    (void)filename;
    return std::vector<char>();
}

std::string get_first_four_chars(std::string filename) {
    (void)filename;
    return "";
}

std::vector<unsigned char> get_file_buffer(std::string filename) {
    (void)filename;
    return std::vector<unsigned char>();
}
//https://stackoverflow.com/a/8518855
std::string getDirectory(const std::string& fname){
    size_t pos = fname.find_last_of("\\/");
    return (std::string::npos == pos)
        ? ""
        : fname.substr(0, pos);
}

bool isAbsolutePath (std::string const &path) {
    if (path.length() == 0) {
        return false;
    }

    if (path[0] == '/') {
        return true;
    }
    
    size_t colonPos = path.find_first_of(":");
    size_t slashPos = path.find_first_of("/");
    size_t backSlashPos = path.find_first_of("\\");

    if (colonPos != std::string::npos && 
        (slashPos == (colonPos + 1) || backSlashPos == (colonPos + 1))){
        return true;
    }
    
    return false;
}

std::string getFileExtension(std::string const &path) {
    size_t pos = path.find_last_of(".");
     return (std::string::npos == pos)
         ? ""
         : path.substr(pos);
}

bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

bool isHiddenFile (std::string const &fullString) {
    if (fullString.length() >= 0) {
        return (0 == fullString.compare (0, 2, "._"));
    } else {
        return false;
    }
}

bool isCartFile (std::string const &fullString) {
    return !isHiddenFile(fullString) && 
        (hasEnding(fullString, ".p8") || hasEnding(fullString, ".png"));
}

bool isCPostFile (std::string const &fullString) {
    return !isHiddenFile(fullString) && 
       fullString.rfind("cpost", 0) == 0;
}
