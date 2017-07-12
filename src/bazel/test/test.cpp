#include <fstream>
#include <string>

#include "../driver.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	std::string f, s;
    std::ifstream ifile(argv[1]);
	while (std::getline(ifile, s))
		f += s + "\n";

    BazelParserDriver driver;
    driver.can_throw = false;
    //driver.debug = true;
	auto ret = driver.parse(f);

	if (ret)
		return ret;

	auto bf = driver.bazel_file;
    bf.trimQuotes();
    auto files = bf.getFiles("protobuf");

    bf = bazel::parse(f);
    files = bf.getFiles("protobuf");
    files = bf.getFiles("protoc_lib");
    files = bf.getFiles("protobuf_lite");
    files = bf.getFiles("protoc");
    files = bf.getFiles("");

    return 0;
}
