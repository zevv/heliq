#pragma once

#include <stdio.h>
#include <string.h>
#include <vector>
#include <functional>
#include <map>


struct ConfigWriter {
public:

	ConfigWriter();

	void open(const char *fname);
	void close();

	void push(const char *key);
	void push(int key);
	void pop();

	void write(const char *key, bool val);
	void write(const char *key, int val);
	void write(const char *key, size_t val);
	void write(const char *key, float val);
	void write(const char *key, double val);
	void write(const char *key, const char *val);
	
private:
	FILE *m_f;
	std::vector<char *> m_prefixes;
};


struct str_cmp {
    bool operator()(const char* a, const char* b) const {
        return strcmp(a, b) < 0;
    }
};


struct ConfigReader {

	class Node {
	public:
		Node();
		~Node();
		Node *find(const char *key);
		void read(const char *key, bool &val);
		void read(const char *key, int &val);
		void read(const char *key, size_t &val);
		void read(const char *key, float &val);
		void read(const char *key, double &val);
		void read(const char *key, char *val, size_t maxlen);
		const char *read_str(const char *key);
		void dump(int depth = 0);

		std::map<const char *, Node *, str_cmp> kids;
		std::map<const char *, const char *, str_cmp> m_attrs;
	};

public:

	ConfigReader();
	~ConfigReader();

	void open(const char *fname);
	void dump();
	void close();


	Node *find(const char *key);
	Node *find(Node *node, const char *key);

private:
	void parse_line(char *line);
	Node *m_root;
	Node *m_cursor;
};



