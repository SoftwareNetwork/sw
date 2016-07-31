#include <fstream>
#include <iostream>
#include <stack>
#include <vector>

#include <context.h>

#include <json.hpp>

template <class K, class V>
struct kv_pair
{
	K first;
	V second;
};

template <class K, class V, class ... Args>
struct my_map : public std::vector<kv_pair<K, V>>
{
	using base = std::vector<kv_pair<K, V>>;

	using key_type = K;

	using iterator = typename base::iterator;
	using const_iterator = typename base::const_iterator;

	iterator find(const key_type &key)
	{
		iterator it;
		for (it = begin(); it != end(); ++it)
		{
			if (it->first == key)
				return it;
		}
		return it;
	}
	const_iterator find(const key_type &key) const
	{
		const_iterator it;
		for (it = begin(); it != end(); ++it)
		{
			if (it->first == key)
				return it;
		}
		return it;
	}

	V& operator[](const key_type &key)
	{
		// add element if missing
		auto i = find(key);
		if (i != end())
			return i->second;
		value_type v;
		v.first = key;
		emplace_back(v);
		return back().second;
	}
	const V& operator[](const key_type &key) const
	{
		return find(key)->second;
	}
};

//using json = nlohmann::json; // will sort keys
using json = nlohmann::basic_json<my_map>; // no sorting

enum class var_type
{
	single,
	set,
	map,
	kv_map,
};

var_type getVarType(json &v)
{
	std::string t = v["type"];
#define IF(x) if (t == #x) return var_type::x
	IF(single);
	IF(set);
	IF(map);
	IF(kv_map);
#undef IF
	throw std::runtime_error("unknown type: " + t);
}

class StackVarName
{
	using T = std::string;

public:
	StackVarName(const T &s)
	{
		names.push(s);
	}

	T pop()
	{
		auto old = get();
		names.pop();
		return old;
	}

	T push()
	{
		auto old = get();
		auto s = old;
		if (isdigit(s.back()))
		{
			auto last_index = s.find_last_not_of("0123456789");
			auto result = s.substr(last_index + 1);
			auto i = std::stoi(result);
			auto n = std::to_string(++i);
			s = s.substr(0, last_index + 1) + n;
		}
		else
			s += "1";
		names.push(s);
		return old;
	}

	T get()
	{
		return names.top();
	}

private:
	std::stack<T> names;
};

class Printer
{
public:
	virtual ~Printer() {}

	virtual void init() = 0;
	virtual void finish() = 0;

	virtual void initSubVar(const std::string &key, json &v) = 0;
	virtual void finishSubVar(const std::string &key, json &v) = 0;

	virtual void printVariable(const std::string &key, json &root) = 0;

	const Context &getContext() const { return ctx; }

	void generate(json &root)
	{
		init();
		generate_variables(root);
		finish();
	}

	void generate_variables(json &root)
	{
		auto &v = root.find("variables");
		if (v == root.end())
			return;
		auto &vv = v.value();
		for (auto it = vv.begin(); it != vv.end(); ++it)
		{
			if (it.value().find("variables") != it.value().end())
			{
				initSubVar(it.key(), it.value());
				generate_variables(it.value());
				finishSubVar(it.key(), it.value());
				if (it.key() == "projects")
				{
					ctx.beginBlock();
					type_var_name.push();
					ctx.addLine("Project " + type_var_name.get() + ";");
					ctx.addLine();
					generate_variables(it.value());
					auto prev = type_var_name.pop();
					ctx.addLine(prev + ".cppan_filename = path_.filename().string();");
					ctx.addLine(prev + ".package = relative_name_to_absolute(\"\");");
					ctx.addLine(type_var_name.get() + ".projects[\"\"] = " + prev + ";");
					ctx.endBlock();
				}
				continue;
			}
			printVariable(it.key(), it.value());
		}
	}

protected:
	Context ctx;
	StackVarName node_var_name{ "r" };
	StackVarName type_var_name{ "c" };
	StackVarName value_var_name{ "v" };
	StackVarName key_var_name{ "k" };
};

using Printers = std::vector<Printer*>;

class YamlPrinter final : public Printer
{
public:
	void init() override
	{
		ctx.beginFunction("void Config::parse(YAML::Node &r, Config &c, const path &path_)");
	}

	void finish() override
	{
		ctx.endFunction();
	}

	void printVariable(const std::string &key, json &v) override
	{
		auto node_prev = node_var_name.push();

		ctx.beginBlock();
		ctx.addLine("std::string " + key_var_name.get() + " = \"" + key + "\";");
		ctx.addLine("auto &" + node_var_name.get() + " = " + node_prev + "[" + key_var_name.get() + "];");
		ctx.beginBlock("if (" + node_var_name.get() + ".IsDefined())");

		printVariableInternal(key, v);

		// remove nodes to prevent subsequent reads
		ctx.emptyLines(1);
		ctx.addLine(node_prev + ".remove(" + node_var_name.get() + ");");

		ctx.endBlock();
		ctx.endBlock();
		ctx.addLine();

		node_var_name.pop();
	}

	void initSubVar(const std::string &key, json &v) override
	{
		auto node_prev = node_var_name.push();

		auto t = getVarType(v);
		std::string dt = v["datatype"];

		ctx.beginBlock();
		ctx.addLine("std::string " + key_var_name.get() + " = \"" + key + "\";");
		ctx.addLine("auto &" + node_var_name.get() + " = " + node_prev + "[" + key_var_name.get() + "];");
		ctx.beginBlock("if (" + node_var_name.get() + ".IsDefined())");

		auto key_prev = key_var_name.push();
		ctx.beginBlock(ifVar(t, node_var_name.get(), false));
		ctx.addLine("auto " + key_var_name.get() + " = \"'\" + " + key_prev + " + \"'\";");
		ctx.addLine("throw std::runtime_error(" + key_var_name.get() + " + \" should be a " + getErrorType(t) + "\");");
		ctx.endBlock();
		ctx.addLine();
		key_var_name.pop();

		switch (t)
		{
		case var_type::map:
		{
			auto prev = node_var_name.push();
			ctx.addLine("auto &" + node_var_name.get() + " = " + prev + ";");
		}
			break;
		}

		type_var_name.push();

		if (t == var_type::kv_map)
		{
			auto prev = node_var_name.push();
			auto key_prev = key_var_name.push();
			ctx.beginBlock("for (const auto &" + node_var_name.get() + " : " + prev + ")");
			ctx.addLine("std::pair<" + dt + "::key_type, " + dt + "::mapped_type> " + type_var_name.get() + ";");
			ctx.addLine(type_var_name.get() + ".first = " + node_var_name.get() + ".first.template as<String>();");
			ctx.addLine();
			prev = node_var_name.push();
			ctx.addLine("auto " + key_var_name.get() + " = " + type_var_name.get() + ".first;");
			auto type_prev = type_var_name.push();
			ctx.addLine("auto &" + type_var_name.get() + " = " + type_prev + ".second;");
			ctx.addLine("auto &" + node_var_name.get() + " = " + prev + ".second;");
			ctx.addLine();
			node_var_name.pop();
			node_var_name.pop();
		}
		else
		{
			ctx.addLine(dt + " " + type_var_name.get() + ";");
			ctx.addLine();
		}

		value_var_name.push();
	}

	void finishSubVar(const std::string &key, json &v) override
	{
		value_var_name.pop();

		auto t = getVarType(v);

		std::string k = key;
		if (!v["variable"].empty())
			k = v["variable"];

		switch (t)
		{
		case var_type::map:
			node_var_name.pop();
			break;
		}

		auto prev = type_var_name.pop();

		if (t == var_type::kv_map)
		{
			prev = type_var_name.pop();
			if (key == "projects")
			{
				ctx.addLine(prev + ".second.cppan_filename = path_.filename().string();");
				ctx.addLine(prev + ".second.package = relative_name_to_absolute(" + prev + ".first);");
				ctx.addLine(prev + ".first = " + prev + ".second.package.toString();");
			}
			ctx.addLine(type_var_name.get() + "." + k + ".insert(" + prev + ");");
		}
		else
			ctx.addLine(type_var_name.get() + "." + k + " = " + prev + ";");

		switch (t)
		{
		case var_type::kv_map:
			ctx.endBlock();
			break;
		}

		ctx.endBlock();
		ctx.endBlock();
		ctx.addLine();

		node_var_name.pop();
	}

private:
	std::string ifVar(var_type t, const std::string &v, bool pos = true)
	{
		std::string n(pos ? "" : "!");
		switch (t)
		{
		case var_type::single:
			return "if (" + n + v + ".IsScalar())";
		case var_type::set:
			return "if (" + n + v + ".IsSequence())";
		case var_type::map:
		case var_type::kv_map:
			return "if (" + n + v + ".IsMap())";
		default:
			throw std::runtime_error("unknown type");
		}
	}

	std::string getErrorType(var_type t)
	{
		switch (t)
		{
		case var_type::single:
			return "scalar";
		case var_type::set:
			return "sequence";
		case var_type::map:
		case var_type::kv_map:
			return "map";
		default:
			throw std::runtime_error("unknown type");
		}
	}

	void printVariableInternal(const std::string &key, json &v)
	{
		const auto t = getVarType(v);

		std::string dt = v["datatype"];
		std::string idt = dt;
		if (!v["internal_datatype"].empty())
			idt = v["internal_datatype"];

		std::string variable = key;
		if (!v["variable"].empty())
			variable = v["variable"];

		std::string access;
		if (!v["access"].empty())
		{
			access = v["access"];
			access = "." + access;
		}

		auto key_prev = key_var_name.push();
		ctx.beginBlock(ifVar(t, node_var_name.get(), false));
		ctx.addLine("auto " + key_var_name.get() + " = \"'\" + " + key_prev + " + \"'\";");
		ctx.addLine("throw std::runtime_error(" + key_var_name.get() + " + \" should be a " + getErrorType(t) + "\");");
		ctx.endBlock();
		ctx.addLine();
		key_var_name.pop();

		switch (t)
		{
		case var_type::single:
			ctx.addLine("auto cv = " + node_var_name.get() + ".template as<" + dt + ">();");
			break;
		case var_type::set:
			ctx.addLine("std::set<" + idt + "> cv;");
			ctx.addLine("for (const auto &x : " + node_var_name.get() + ")");
			ctx.increaseIndent();
			ctx.addLine("cv.insert(x.template as<" + dt + ">());");
			ctx.decreaseIndent();
			break;
		case var_type::kv_map:
		{
			auto prev = node_var_name.push();
			auto type_prev = type_var_name.push();
			auto key_prev = key_var_name.push();
			ctx.beginBlock("for (const auto &" + node_var_name.get() + " : " + prev + ")");
			ctx.addLine("std::pair<" + idt + "::key_type, " + idt + "::mapped_type> " + type_var_name.get() + ";");
			ctx.addLine(type_var_name.get() + ".first = " + node_var_name.get() + ".first.template as<String>();");
			ctx.addLine();
			prev = node_var_name.push();
			ctx.addLine("auto " + key_var_name.get() + " = " + type_var_name.get() + ".first;");
			ctx.addLine("auto &" + node_var_name.get() + " = " + prev + ".second;");
			ctx.addLine();
			node_var_name.pop();
			node_var_name.pop();
		}
		break;
		}

		if (!v["default"].empty())
		{
			ctx.addLine("if (cv.empty())");
			ctx.increaseIndent();
			std::string d = v["default"];
			ctx.addLine("cv = \"" + d + "\";");
			ctx.decreaseIndent();
		}

		if (t == var_type::kv_map)
		{
			auto prev = type_var_name.pop();
			ctx.addLine(type_var_name.get() + access + "." + variable + ".insert(" + prev + ");");
			ctx.endBlock();
		}
		else
		{
			if (v["apply"].empty())
				ctx.addLine(type_var_name.get() + access + "." + variable + " = cv;");
			else
			{
				std::string apply = v["apply"];
				ctx.addLine(type_var_name.get() + access + "." + variable + " = " + apply + "(cv);");
			}
		}
	}
};

class JsonPrinter final : public Printer
{

};

int main(int argc, char *argv[])
try
{
	if (argc != 3)
		return 1;

	Printers printers;
	printers.push_back(new YamlPrinter);

	std::ifstream ifile(argv[1]);
	json root(ifile);
	for (auto &p : printers)
		p->generate(root);

	std::ofstream ofile(argv[2]);
	for (auto &p : printers)
		ofile << p->getContext().getText();

	return 0;
}
catch (std::exception &e)
{
	std::cerr << "exception: " << e.what() << "\n";
	return 1;
}
