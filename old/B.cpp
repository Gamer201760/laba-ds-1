#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
using namespace std;

struct Node {
	string key;
	uint64_t val;
	int h;
	Node *l, *r;
};

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

Node *insert(Node *v, const string &key, uint64_t val, bool &added) {
	if (!v) {
		Node *nd = new Node;
		nd->key = key;
		nd->val = val;
		nd->h = 1;
		nd->l = nd->r = nullptr;
		added = true;
		return nd;
	}
	int c = key.compare(v->key);
	if (c == 0) {
		added = false;
		return v;
	}
	if (c < 0)
		v->l = insert(v->l, key, val, added);
	else
		v->r = insert(v->r, key, val, added);
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

Node *erase(Node *v, const string &key, bool &removed) {
	if (!v) {
		removed = false;
		return nullptr;
	}
	int c = key.compare(v->key);
	if (c < 0) {
		v->l = erase(v->l, key, removed);
	} else if (c > 0) {
		v->r = erase(v->r, key, removed);
	} else {
		removed = true;
		Node *l = v->l, *r = v->r;
		delete v;
		if (!r)
			return l;
		Node *mn;
		r = removeMin(r, mn);
		mn->l = l;
		mn->r = r;
		return balance(mn);
	}
	return balance(v);
}

Node *find(Node *v, const string &key) {
	while (v) {
		int c = key.compare(v->key);
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
	delete v;
}

const char MAGIC[4] = {'A', 'V', 'L', '1'};

uint64_t countNodes(Node *v) {
	return v ? countNodes(v->l) + countNodes(v->r) + 1 : 0;
}

bool saveNodes(Node *v, FILE *f) {
	if (!v)
		return true;
	uint16_t len = v->key.size();
	if (fwrite(&len, sizeof(len), 1, f) != 1)
		return false;
	if (fwrite(v->key.data(), 1, len, f) != len)
		return false;
	if (fwrite(&v->val, sizeof(v->val), 1, f) != 1)
		return false;
	return saveNodes(v->l, f) && saveNodes(v->r, f);
}

string save(Node *root, const string &path) {
	FILE *f = fopen(path.c_str(), "wb");
	if (!f)
		return string("ERROR: ") + strerror(errno);
	uint64_t n = countNodes(root);
	bool ok = fwrite(MAGIC, 1, 4, f) == 4 &&
			  fwrite(&n, sizeof(n), 1, f) == 1 && saveNodes(root, f);
	int e = errno;
	if (fclose(f) != 0 && ok) {
		ok = false;
		e = errno;
	}
	if (!ok)
		return string("ERROR: ") + strerror(e);
	return "OK";
}

string load(Node *&root, const string &path) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) {
		if (errno == ENOENT) {
			clear(root);
			root = nullptr;
			return "OK";
		}
		return string("ERROR: ") + strerror(errno);
	}

	char hdr[4];
	size_t got = fread(hdr, 1, 4, f);
	if (got == 0 && feof(f)) {
		fclose(f);
		clear(root);
		root = nullptr;
		return "OK";
	}

	Node *t = nullptr;
	string res = "OK";
	uint64_t n;
	if (got != 4 || memcmp(hdr, MAGIC, 4) != 0 ||
		fread(&n, sizeof(n), 1, f) != 1) {
		res = "ERROR: Wrong file format";
	} else {
		try {
			char buf[257];
			for (uint64_t i = 0; i < n; i++) {
				uint16_t len;
				uint64_t val;
				if (fread(&len, sizeof(len), 1, f) != 1 || len == 0 ||
					len > 256 || fread(buf, 1, len, f) != len ||
					fread(&val, sizeof(val), 1, f) != 1) {
					res = "ERROR: Wrong file format";
					break;
				}
				bool good = true;
				for (int j = 0; j < len; j++)
					if (buf[j] < 'a' || buf[j] > 'z')
						good = false;
				bool added = false;
				if (good)
					t = insert(t, string(buf, len), val, added);
				if (!added) {
					res = "ERROR: Wrong file format";
					break;
				}
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
		res = string("ERROR: ") + strerror(errno);
	fclose(f);

	if (res == "OK") {
		clear(root);
		root = t;
	} else {
		clear(t);
	}
	return res;
}

string lower(string s) {
	for (int i = 0; i < (int)s.size(); i++)
		if (s[i] >= 'A' && s[i] <= 'Z')
			s[i] = s[i] - 'A' + 'a';
	return s;
}

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);
	Node *root = nullptr;
	string line;
	while (getline(cin, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			continue;
		try {
			if (line[0] == '+') {
				istringstream in(line.substr(1));
				string w;
				uint64_t v;
				in >> w >> v;
				bool added;
				root = insert(root, lower(w), v, added);
				cout << (added ? "OK" : "Exist") << '\n';
			} else if (line[0] == '-') {
				istringstream in(line.substr(1));
				string w;
				in >> w;
				bool removed;
				root = erase(root, lower(w), removed);
				cout << (removed ? "OK" : "NoSuchWord") << '\n';
			} else if (line[0] == '!') {
				istringstream in(line.substr(1));
				string cmd, path;
				in >> cmd;
				getline(in >> ws, path);
				if (cmd == "Save")
					cout << save(root, path) << '\n';
				else
					cout << load(root, path) << '\n';
			} else {
				Node *v = find(root, lower(line));
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
