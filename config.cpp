
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "config.hpp"

ConfigWriter::ConfigWriter() 
	: m_f(nullptr)
	, m_prefixes()
{
	
}


void ConfigWriter::open(const char *fname) 
{
	m_f = fopen(fname, "w");
	if(m_f == nullptr) {
		fprintf(stderr, "Could not open config file for writing: %s\n", fname);
		return;
	}
}


void ConfigWriter::close() 
{
	if(m_f) {
		fclose(m_f);
		m_f = nullptr;
	}
}


void ConfigWriter::push(int key)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "[%d]", key);
	push(buf);
}

	
void ConfigWriter::push(const char *key)
{
	m_prefixes.push_back(strdup(key));
}


void ConfigWriter::pop()
{
	if(!m_prefixes.empty()) {
		free(m_prefixes.back());
		m_prefixes.pop_back();
	}
}


void ConfigWriter::write(const char *key, bool val)
{
	write(key, val ? "true" : "false");
}


void ConfigWriter::write(const char *key, int val)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", val);
	write(key, buf);
}


void ConfigWriter::write(const char *key, float val)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%.7g", val);
	write(key, buf);
}


void ConfigWriter::write(const char *key, double val)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%.7g", val);
	write(key, buf);
}

void ConfigWriter::write(const char *key, const char *val)
{
	if(m_f) {
		size_t i = 0;
		for(auto p : m_prefixes) {
			if(i++ >0 && p[0] != '[') fprintf(m_f, ".");
			fprintf(m_f, "%s", p);
		}
		fprintf(m_f, ".");
		fprintf(m_f, "%s=%s\n", key, val);
	}
}


ConfigReader::ConfigReader() 
{
	m_root = new Node();
	m_cursor = m_root;
}


void ConfigReader::open(const char *fname) 
{
	FILE *f = fopen(fname, "r");
	if(f == nullptr) {
		fprintf(stderr, "Could not open config file for reading: %s\n", fname);
		return;
	}

	char buf[256];
	while(fgets(buf, sizeof(buf), f)) {
		char *nl = strchr(buf, '\n');
		if(nl) *nl = 0;
		parse_line(buf);
	}
	fclose(f);
}


void ConfigReader::dump()
{
	m_root->dump();
}


ConfigReader::~ConfigReader()
{
	delete m_root;
}


void ConfigReader::close()
{
}


static ConfigReader::Node nullnode;

ConfigReader::Node::Node()
	: kids()
	, m_attrs()
{}



ConfigReader::Node::~Node()
{
	for(auto it : m_attrs) {
		if(it.first) free((void *)it.first);
		if(it.second) free((void *)it.second);
	}
	for(auto it : kids) {
		if(it.first) free((void *)it.first);
		delete it.second;
	}
}


void ConfigReader::Node::dump(int depth)
{
	for(auto it : m_attrs) {
		for(int i=0; i<depth; i++) printf("  ");
		printf("- %s = '%s'\n", it.first, it.second);
	}
	for(auto it : kids) {
		for(int i=0; i<depth; i++) printf("  ");
		printf("- %s\n", it.first);
		it.second->dump(depth + 1);
	}
}


ConfigReader::Node *ConfigReader::find(const char *key)
{
	return find(m_root, key);
}


ConfigReader::Node *ConfigReader::find(Node *node, const char *key)
{
	static Node	nullnode;
	if(node->kids.find(key) != node->kids.end()) {
		return node->kids[key];
	} else {
		return &nullnode;
	}
}


void ConfigReader::parse_line(char *line)
{
	char *eq = strchr(line, '=');
	if(eq == nullptr) return;
	*eq = '\0';
	const char *val = eq + 1;
	Node * node = m_root;

	char *save;
	char *key = strtok_r(line, ".[]", &save);
	while(key) {
		char *key_next = strtok_r(nullptr, ".[]", &save);
		if(key_next) {
			if(node->kids.find(key) == node->kids.end()) {
				node->kids[strdup(key)] = new Node();
			}
			node = node->kids[key];
		} else {
			node->m_attrs[strdup(key)] = strdup(val);
			break;
		}
		key = key_next;
	}
}

		
ConfigReader::Node *ConfigReader::Node::find(const char *key)
{
	return (kids.find(key) != kids.end()) ? kids[key] : &nullnode;
}


void ConfigReader::Node::read(const char *key, bool &val)
{
	const char *buf = read_str(key);
	if(buf) val = strcmp(buf, "true") == 0;
}


void ConfigReader::Node::read(const char *key, int &val)
{
	const char *buf = read_str(key);
	if(buf) val= atoi(buf);
}


void ConfigReader::Node::read(const char *key, float &val)
{
	const char *buf = read_str(key);
	if(buf) val= atof(buf);
}


void ConfigReader::Node::read(const char *key, double &val)
{
	const char *buf = read_str(key);
	if(buf) val= atof(buf);
}


void ConfigReader::Node::read(const char *key, char *val, size_t maxlen)
{
	auto it = m_attrs.find(key);
	if(it != m_attrs.end()) {
		snprintf(val, maxlen, "%s", it->second);
	}
}


const char *ConfigReader::Node::read_str(const char *key)
{
	auto it = m_attrs.find(key);
	if(it != m_attrs.end()) {
		return it->second;
	} else {
		return nullptr;
	}
}

