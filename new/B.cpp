#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace std;

const int MAXLEN = 256;

struct Node {
	Node *l, *r;
	uint64_t val;
	signed char h;
	unsigned char len;
	char key[1];
};

int keyLen(Node *v) {
	return v->len + 1;
}

Node *newNode(const char *key, int len, uint64_t val) {
	Node *nd = (Node *)malloc(offsetof(Node, key) + len);
	if (!nd)
		throw bad_alloc();
	nd->l = nd->r = nullptr;
	nd->val = val;
	nd->h = 1;
	nd->len = len - 1;
	memcpy(nd->key, key, len);
	return nd;
}

int cmp(const char *key, int len, Node *v) {
	int vl = keyLen(v);
	int c = memcmp(key, v->key, len < vl ? len : vl);
	if (c != 0)
		return c;
	return len - vl;
}

int height(Node *v) {
	return v ? v->h : 0;
}

void fix(Node *v) {
	int a = height(v->l), b = height(v->r);
	v->h = (a > b ? a : b) + 1;
}

Node *rotRight(Node *v) {
	Node *u = v->l;
	v->l = u->r;
	u->r = v;
	fix(v);
	fix(u);
	return u;
}

Node *rotLeft(Node *v) {
	Node *u = v->r;
	v->r = u->l;
	u->l = v;
	fix(v);
	fix(u);
	return u;
}

Node *balance(Node *v) {
	fix(v);
	int d = height(v->l) - height(v->r);
	if (d == 2) {
		if (height(v->l->l) < height(v->l->r))
			v->l = rotLeft(v->l);
		return rotRight(v);
	}
	if (d == -2) {
		if (height(v->r->r) < height(v->r->l))
			v->r = rotRight(v->r);
		return rotLeft(v);
	}
	return v;
}

Node *insert(Node *v, const char *key, int len, uint64_t val, bool &added) {
	if (!v) {
		added = true;
		return newNode(key, len, val);
	}
	int c = cmp(key, len, v);
	if (c == 0) {
		added = false;
		return v;
	}
	if (c < 0)
		v->l = insert(v->l, key, len, val, added);
	else
		v->r = insert(v->r, key, len, val, added);
	if (!added)
		return v;
	return balance(v);
}

Node *removeMin(Node *v, Node *&mn) {
	if (!v->l) {
		mn = v;
		return v->r;
	}
	v->l = removeMin(v->l, mn);
	return balance(v);
}

Node *erase(Node *v, const char *key, int len, bool &removed) {
	if (!v) {
		removed = false;
		return nullptr;
	}
	int c = cmp(key, len, v);
	if (c < 0) {
		v->l = erase(v->l, key, len, removed);
	} else if (c > 0) {
		v->r = erase(v->r, key, len, removed);
	} else {
		removed = true;
		Node *l = v->l, *r = v->r;
		free(v);
		if (!r)
			return l;
		Node *mn;
		r = removeMin(r, mn);
		mn->l = l;
		mn->r = r;
		return balance(mn);
	}
	if (!removed)
		return v;
	return balance(v);
}

Node *find(Node *v, const char *key, int len) {
	while (v) {
		int c = cmp(key, len, v);
		if (c == 0)
			return v;
		v = c < 0 ? v->l : v->r;
	}
	return nullptr;
}

void clear(Node *v) {
	if (!v)
		return;
	clear(v->l);
	clear(v->r);
	free(v);
}

const char MAGIC[4] = {'A', 'V', 'L', '2'};

uint64_t countNodes(Node *v) {
	return v ? countNodes(v->l) + countNodes(v->r) + 1 : 0;
}

bool saveNodes(Node *v, FILE *f) {
	if (!v)
		return true;
	if (!saveNodes(v->l, f))
		return false;
	if (fwrite(&v->len, 1, 1, f) != 1)
		return false;
	if (fwrite(v->key, 1, keyLen(v), f) != (size_t)keyLen(v))
		return false;
	if (fwrite(&v->val, sizeof(v->val), 1, f) != 1)
		return false;
	return saveNodes(v->r, f);
}

string sysError(int e) {
	return string("ERROR: ") + strerror(e);
}

string save(Node *root, const string &path) {
	string tmp = path + ".tmp";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f)
		return sysError(errno);
	uint64_t n = countNodes(root);
	bool ok = fwrite(MAGIC, 1, 4, f) == 4 &&
			  fwrite(&n, sizeof(n), 1, f) == 1 && saveNodes(root, f);
	int e = errno;
	if (fclose(f) != 0 && ok) {
		ok = false;
		e = errno;
	}
	if (ok && rename(tmp.c_str(), path.c_str()) != 0) {
		ok = false;
		e = errno;
	}
	if (!ok) {
		remove(tmp.c_str());
		return sysError(e);
	}
	return "OK";
}

Node *link(vector<Node *> &a, int lo, int hi) {
	if (lo >= hi)
		return nullptr;
	int m = (lo + hi) / 2;
	Node *v = a[m];
	v->l = link(a, lo, m);
	v->r = link(a, m + 1, hi);
	fix(v);
	return v;
}

string load(Node *&root, const string &path) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) {
		if (errno == ENOENT) {
			clear(root);
			root = nullptr;
			return "OK";
		}
		return sysError(errno);
	}

	char hdr[4];
	size_t got = fread(hdr, 1, 4, f);
	if (got == 0 && feof(f)) {
		fclose(f);
		clear(root);
		root = nullptr;
		return "OK";
	}

	vector<Node *> a;
	string res = "OK";
	uint64_t n;
	if (got != 4 || memcmp(hdr, MAGIC, 4) != 0 ||
		fread(&n, sizeof(n), 1, f) != 1) {
		res = "ERROR: Wrong file format";
	} else {
		try {
			char buf[MAXLEN];
			for (uint64_t i = 0; i < n; i++) {
				unsigned char l;
				uint64_t val;
				if (fread(&l, 1, 1, f) != 1 || fread(buf, 1, l + 1, f) != l + 1u ||
					fread(&val, sizeof(val), 1, f) != 1) {
					res = "ERROR: Wrong file format";
					break;
				}
				int len = l + 1;
				bool good = true;
				for (int j = 0; j < len; j++)
					if (buf[j] < 'a' || buf[j] > 'z')
						good = false;
				if (good && !a.empty() && cmp(buf, len, a.back()) <= 0)
					good = false;
				if (!good) {
					res = "ERROR: Wrong file format";
					break;
				}
				a.push_back(newNode(buf, len, val));
			}
			if (res == "OK") {
				char c;
				if (fread(&c, 1, 1, f) == 1)
					res = "ERROR: Wrong file format";
			}
		} catch (bad_alloc &) {
			res = "ERROR: Not enough memory";
		}
	}
	if (res == "OK" && ferror(f))
		res = sysError(errno);
	fclose(f);

	if (res != "OK") {
		for (int i = 0; i < (int)a.size(); i++)
			free(a[i]);
		return res;
	}
	clear(root);
	root = link(a, 0, a.size());
	return res;
}

void skipSpaces(const string &s, size_t &i) {
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
		i++;
}

bool readWord(const string &s, size_t &i, char *buf, int &len) {
	skipSpaces(s, i);
	len = 0;
	while (i < s.size() && s[i] != ' ' && s[i] != '\t') {
		char c = s[i];
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
		if (c < 'a' || c > 'z')
			return false;
		if (len <= MAXLEN)
			buf[len++] = c;
		i++;
	}
	return len > 0;
}

bool readNum(const string &s, size_t &i, uint64_t &v) {
	skipSpaces(s, i);
	v = 0;
	size_t st = i;
	while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
		uint64_t d = s[i] - '0';
		if (v > (UINT64_MAX - d) / 10)
			return false;
		v = v * 10 + d;
		i++;
	}
	return i > st;
}

bool atEnd(const string &s, size_t &i) {
	skipSpaces(s, i);
	return i == s.size();
}

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);
	Node *root = nullptr;
	string line;
	char w[MAXLEN + 1];
	int len;
	while (getline(cin, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t i = 0;
		if (atEnd(line, i))
			continue;
		try {
			if (line[i] == '+') {
				i++;
				uint64_t v;
				if (!readWord(line, i, w, len) || len > MAXLEN ||
					!readNum(line, i, v) || !atEnd(line, i)) {
					cout << "ERROR: Invalid command\n";
					continue;
				}
				bool added;
				root = insert(root, w, len, v, added);
				cout << (added ? "OK" : "Exist") << '\n';
			} else if (line[i] == '-') {
				i++;
				if (!readWord(line, i, w, len) || !atEnd(line, i)) {
					cout << "ERROR: Invalid command\n";
					continue;
				}
				bool removed;
				root = erase(root, w, len, removed);
				cout << (removed ? "OK" : "NoSuchWord") << '\n';
			} else if (line[i] == '!') {
				i++;
				skipSpaces(line, i);
				size_t st = i;
				while (i < line.size() && line[i] != ' ' && line[i] != '\t')
					i++;
				string cmd = line.substr(st, i - st);
				skipSpaces(line, i);
				string path = line.substr(i);
				if (path.empty() || (cmd != "Save" && cmd != "Load"))
					cout << "ERROR: Invalid command\n";
				else if (cmd == "Save")
					cout << save(root, path) << '\n';
				else
					cout << load(root, path) << '\n';
			} else {
				if (!readWord(line, i, w, len) || !atEnd(line, i)) {
					cout << "ERROR: Invalid command\n";
					continue;
				}
				Node *v = find(root, w, len);
				if (v)
					cout << "OK: " << v->val << '\n';
				else
					cout << "NoSuchWord\n";
			}
		} catch (bad_alloc &) {
			cout << "ERROR: Not enough memory\n";
		}
	}
	clear(root);
}
