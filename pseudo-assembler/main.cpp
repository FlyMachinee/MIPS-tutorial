#include <iostream>
#include <ios>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <ctype.h>

using namespace std;

enum AddrType
{
    RELATIVE,
    ABSOLUTE
};

void exec(istream &is, const string &symbol, ostream &os);
void translate_r_type(const string &symbol, int rs, int rt, int rd, int shamt, ostream &os);
void translate_i_type(const string &symbol, int rs, int rt, int imm, ostream &os);
void translate_j_type(const string &symbol, int addr, ostream &os);
void translate_compound_type(istream &is, const string &symbol, ostream &os);

int get_num(istream &is, AddrType type = RELATIVE);
int get_reg(istream &is);
int get_reg_by_alias(const string &reg_alias);
string unsigned_dec_to_bin(unsigned int n);
string dec_to_bin(int n, int bin_size);
string byte_to_hex(const string &byte);
int hex_to_dec(const string &hex);
int bin_to_dec(const string &bin);
int bin_to_signed_dec(const string &bin);
string dec_to_dword(unsigned int num);
string to_hex_line(const string &bit_line);
void pre_process(const char *filename);
string line_head();
string spaced_str(const string &str, int len, bool right_blank = true);

const vector<string> reg_alias_list({"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"});
map<string, int> reg_custom_alias_map;
map<string, int> label_addr_map;
map<int, string> addr_label_map;
const set<string> r_type({"add", "sub", "and", "or", "xor", "slt", "seq", "sgt", "sll", "srl", "sra", "jr"});
const set<string> i_type({"addi", "subi", "andi", "ori", "xori", "slti", "seqi", "sgti", "slli", "srli", "srai", "lw", "sw", "beq"});
const set<string> j_type({"j", "jal"});
const set<string> compound_type({"inc", "dec", "cls", "mov", "movi", "hlt", "push", "pop", "pushi", "popi", "call", "ret"});
const map<string, int> compound_type_size({{"inc", 1}, {"dec", 1}, {"cls", 1}, {"mov", 1}, {"movi", 1}, {"hlt", 1}, {"push", 2}, {"pop", 2}, {"pushi", 3}, {"popi", 1}, {"call", 5}, {"ret", 1}});
const map<string, string> get_op({{"addi", "000001"},
                                  {"subi", "000010"},
                                  {"andi", "000011"},
                                  {"ori", "000100"},
                                  {"xori", "000101"},
                                  {"slti", "001001"},
                                  {"seqi", "001010"},
                                  {"sgti", "001011"},
                                  {"slli", "000110"},
                                  {"srli", "000111"},
                                  {"srai", "001000"},
                                  {"lw", "010000"},
                                  {"sw", "010001"},
                                  {"beq", "100000"},
                                  {"j", "100001"},
                                  {"jal", "110000"}});

const map<string, string> get_func({{"add", "000001"},
                                    {"sub", "000010"},
                                    {"and", "000011"},
                                    {"or", "000100"},
                                    {"xor", "000101"},
                                    {"slt", "001001"},
                                    {"seq", "001010"},
                                    {"sgt", "001011"},
                                    {"sll", "000110"},
                                    {"srl", "000111"},
                                    {"sra", "001000"},
                                    {"jr", "010000"}});
int pc = 0;
int line = 1;
bool print_res = false;

#define DEBUG 0

int main(int argc, const char *argv[])
{
    system("chcp 65001");

#if DEBUG
    pre_process("test.txt");
    print_res = true;
#else
    if (argc < 2)
    {
        cerr << "请输入文件名！\n";
        cerr << "用例: " << argv[0] << " <filename>\n";
        exit(1);
    }
    else if (argc > 2 && argc <= 3)
    {
        if (string(argv[2]) == "-p")
        {
            print_res = true;
        }
        else
        {
            cerr << "参数错误！\n";
            exit(1);
        }
    }
    pre_process(argv[1]);
#endif

    line = 0;
    pc = 0;
    cout << "预处理完毕\n";

#if DEBUG
    ifstream ifs("test.txt");
    ofstream ofs("assembly_test.txt");
#else
    ifstream ifs(argv[1]);
    ofstream ofs("./assembly_" + string(argv[1] + 2));
    if (!ifs)
    {
        cerr << "输入文件" << argv[1] << "打开失败！\n";
        exit(1);
    }

    if (ofs)
    {
        cout << "输出文件已创建\n";
    }
    else
    {
        cerr << "输出文件创建失败！\n";
    }
#endif

    if (print_res)
    {
        cout << "输出结果：\n";
    }

    while (!ifs.eof())
    {
        string symbol;

        while (isspace(ifs.peek()))
            ifs.get();
        if (ifs.peek() == char_traits<char>::eof())
            break;

        if (ifs.peek() == '#') // 是注释
        {
            getline(ifs, symbol);
            ofs << symbol << '\n';
            ++line;
            continue;
        }

        if (ifs.peek() == '!') // 是宏命令
        {
            ifs.get();
            ifs >> symbol;
            if (symbol == "define")
            {
                while (isspace(ifs.peek()))
                    ifs.get();
                if (ifs.get() != '%')
                {
                    cerr << line_head() << "define中的左寄存器必须以%标识！" << endl;
                    exit(1);
                }

                string alias;
                ifs >> alias;

                int reg_num = get_reg(ifs);
                reg_custom_alias_map.insert({alias, reg_num});
                ++line;
                continue;
            }
            else if (symbol == "label")
            {
                getline(ifs, symbol);
                ++line;
                continue;
            }
            else
            {
                cerr << line_head() << "未知宏命令\"" << symbol << "\"！" << endl;
                exit(1);
            }
        }

        ifs >> symbol;
        if (symbol.empty())
            continue;

        if (print_res)
        {
            if (addr_label_map.count(pc))
            {
                string label = addr_label_map.at(pc);
                cout << spaced_str("." + label, 10, false) << ":\n";
            }
        }

        exec(ifs, symbol, ofs);
    }

    return 0;
}

void exec(istream &is, const string &symbol, ostream &os)
{
    int rs = 0, rt = 0, rd = 0, shamt = 0, imm = 0, addr = 0;
    string output_bin;

    if (r_type.count(symbol))
    {
        // R-Type
        if (symbol == "jr")
        {
            rs = get_reg(is);
        }
        else if (symbol == "sll" || symbol == "srl" || symbol == "sra")
        {
            rt = get_reg(is);
            shamt = get_num(is);
            if (shamt > 31)
            {
                cerr << line_head() << "shamt不能大于31！" << endl;
                exit(1);
            }
            else if (shamt < 0)
            {
                cerr << line_head() << "shamt不能小于0！" << endl;
                exit(1);
            }
            rd = get_reg(is);
        }
        else
        {
            rs = get_reg(is);
            rt = get_reg(is);
            rd = get_reg(is);
        }
        translate_r_type(symbol, rs, rt, rd, shamt, os);
    }
    else if (i_type.count(symbol))
    {
        // I-Type
        if (symbol == "beq")
        {
            rs = get_reg(is);
            rt = get_reg(is);
            imm = get_num(is, RELATIVE);
        }
        else
        {
            rs = get_reg(is);
            imm = get_num(is);
            rt = get_reg(is);
        }
        translate_i_type(symbol, rs, rt, imm, os);
    }
    else if (j_type.count(symbol))
    {
        // J-Type
        addr = get_num(is, ABSOLUTE);
        if (addr < 0)
        {
            cerr << line_head() << "地址不能为负！" << endl;
            exit(1);
        }
        translate_j_type(symbol, addr, os);
    }
    else if (compound_type.count(symbol))
    {
        // Compound-Type
        translate_compound_type(is, symbol, os);
    }
    else
    {
        cerr << line_head() << "未知指令\"" << symbol << "\"！" << endl;
        exit(1);
    }

    ++line;
}

void translate_r_type(const string &symbol, int rs, int rt, int rd, int shamt, ostream &os)
{
    string output_bin = "000000" + dec_to_bin(rs, 5) + dec_to_bin(rt, 5) + dec_to_bin(rd, 5) + dec_to_bin(shamt, 5) + get_func.at(symbol);
    os << to_hex_line(output_bin) << '\n';

    if (print_res)
    {
        cout << dec_to_dword(pc * 4) << ": " << spaced_str(symbol, 5);
        if (symbol == "jr")
        {
            cout << '$' << reg_alias_list.at(rs) << endl;
        }
        else if (symbol == "sll" || symbol == "srl" || symbol == "sra")
        {
            cout << '$' << spaced_str(reg_alias_list.at(rt), 5) << shamt << " $" << reg_alias_list.at(rd) << endl;
        }
        else
        {
            cout << '$' << spaced_str(reg_alias_list.at(rs), 5) << '$' << spaced_str(reg_alias_list.at(rt), 5) << '$' << reg_alias_list.at(rd) << endl;
        }
    }

    ++pc;
}

void translate_i_type(const string &symbol, int rs, int rt, int imm, ostream &os)
{
    string output_bin = get_op.at(symbol) + dec_to_bin(rs, 5) + dec_to_bin(rt, 5) + dec_to_bin(imm, 16);
    os << to_hex_line(output_bin) << '\n';

    if (print_res)
    {
        cout << dec_to_dword(pc * 4) << ": " << spaced_str(symbol, 5);
        if (symbol == "beq")
        {
            cout << '$' << spaced_str(reg_alias_list.at(rs), 5) << '$' << spaced_str(reg_alias_list.at(rt), 5) << imm;
            int dest_addr = pc + 1 + imm;
            if (addr_label_map.count(dest_addr))
            {
                cout << "(." << addr_label_map.at(dest_addr) << ')';
            }
            cout << endl;
        }
        else
        {
            cout << '$' << spaced_str(reg_alias_list.at(rs), 5) << imm << " $" << reg_alias_list.at(rt) << endl;
        }
    }
    ++pc;
}

void translate_j_type(const string &symbol, int addr, ostream &os)
{
    string output_bin = get_op.at(symbol) + dec_to_bin(addr, 26);
    os << to_hex_line(output_bin) << '\n';

    if (print_res)
    {
        cout << dec_to_dword(pc * 4) << ": " << spaced_str(symbol, 5) << addr;
        if (addr_label_map.count(addr))
        {
            cout << "(." << addr_label_map.at(addr) << ')';
        }
        cout << endl;
    }
    ++pc;
}

namespace compound
{
    void translate_reg(const string &symbol, int reg, ostream &os)
    {
        if (symbol == "inc")
        {
            translate_i_type("addi", reg, reg, 1, os);
        }
        else if (symbol == "dec")
        {
            translate_i_type("subi", reg, reg, 1, os);
        }
        else if (symbol == "cls")
        {
            translate_r_type("xor", reg, reg, reg, 0, os);
        }
        else if (symbol == "push")
        {
            translate_reg("dec", get_reg_by_alias("sp"), os);
            translate_i_type("sw", get_reg_by_alias("sp"), reg, 0, os);
        }
        else if (symbol == "pop")
        {
            translate_i_type("lw", get_reg_by_alias("sp"), reg, 0, os);
            translate_reg("inc", get_reg_by_alias("sp"), os);
        }
    }

    void translate_reg_reg(const string &symbol, int reg_1, int reg_2, ostream &os)
    {
        if (symbol == "mov")
        {
            translate_i_type("addi", reg_1, reg_2, 0, os);
        }
    }

    void translate_num_reg(const string &symbol, int num, int reg, ostream &os)
    {
        if (symbol == "movi")
        {
            translate_i_type("addi", 0, reg, num, os);
        }
    }

    void translate_void(const string &symbol, ostream &os)
    {
        if (symbol == "hlt")
        {
            translate_i_type("beq", 0, 0, -1, os);
        }
        else if (symbol == "popi")
        {
            translate_reg("inc", get_reg_by_alias("sp"), os);
        }
        else if (symbol == "ret")
        {
            translate_r_type("jr", 31, 0, 0, 0, os);
        }
    }

    void translate_num(const string &symbol, int num, ostream &os)
    {
        if (symbol == "pushi")
        {
            translate_num_reg("movi", num, 1, os);
            translate_reg("push", 1, os);
        }
        else if (symbol == "call")
        {
            translate_reg("push", 31, os);
            translate_j_type("jal", num, os);
            translate_reg("pop", 31, os);
        }
    }
} // namespace compound

void translate_compound_type(istream &is, const string &symbol, ostream &os)
{
    if (symbol == "inc" || symbol == "dec" || symbol == "cls" || symbol == "push" || symbol == "pop")
    {
        // xxx reg
        int reg = get_reg(is);
        compound::translate_reg(symbol, reg, os);
    }
    else if (symbol == "mov")
    {
        // xxx reg_1 reg_2
        int reg_1 = get_reg(is);
        int reg_2 = get_reg(is);
        compound::translate_reg_reg(symbol, reg_1, reg_2, os);
    }
    else if (symbol == "movi")
    {
        // xxx num reg
        int num = get_num(is);
        int reg = get_reg(is);
        compound::translate_num_reg(symbol, num, reg, os);
    }
    else if (symbol == "hlt" || symbol == "popi" || symbol == "ret")
    {
        // xxx
        compound::translate_void(symbol, os);
    }
    else if (symbol == "pushi" || symbol == "call")
    {
        // xxx num
        if (symbol == "pushi")
        {
            int num = get_num(is);
            compound::translate_num(symbol, num, os);
        }
        else if (symbol == "call")
        {
            int num = get_num(is, ABSOLUTE);
            compound::translate_num(symbol, num, os);
        }
    }
}

int get_reg(istream &is)
{
    while (isspace(is.peek()))
        is.get();

    int reg_num;
    if (is.peek() == '$')
    {
        is.get();
        if (isdigit(is.peek())) // 纯数字寄存器
        {
            is >> reg_num;
        }
        else // 默认别名
        {
            string reg_name;
            is >> reg_name;
            reg_num = get_reg_by_alias(reg_name);
        }
    }
    else if (is.peek() == '%') // 自定义寄存器别名
    {
        is.get();
        string reg_name;
        is >> reg_name;
        try
        {
            reg_num = reg_custom_alias_map.at(reg_name);
        }
        catch (const std::exception &e)
        {
            cerr << line_head() << "寄存器自定义别名\"" << reg_name << "\"不存在！" << '\n';
            exit(1);
        }
    }
    else
    {
        cerr << line_head() << "寄存器格式错误！\n";
        exit(1);
    }
    return reg_num;
}

int get_reg_by_alias(const string &reg_alias)
{
    for (int i = 0; i < 31; ++i)
    {
        if (reg_alias == reg_alias_list[i])
            return i;
    }

    cerr << line_head() << "寄存器默认别名\"" << reg_alias << "\"不存在！" << '\n';
    exit(1);
}

int get_num(istream &is, AddrType type)
{
    while (isspace(is.peek()))
        is.get();

    int ret = 0;

    if (is.peek() == '.') // 是标签
    {
        is.get();
        string label;
        is >> label;

        try
        {
            if (type == RELATIVE)
            {
                ret = label_addr_map.at(label) - pc - 1;
            }
            else // ABSOLUTE
            {
                ret = label_addr_map.at(label);
            }
        }
        catch (const std::exception &e)
        {
            cerr << line_head() << "标签\"." + label + "\"未定义！\n";
            exit(1);
        }
    }
    else if (is.peek() == '0')
    {
        is.get();
        if (is.peek() == 'x' || is.peek() == 'X')
        {
            // 十六进制
            is.get();
            string hex;
            is >> hex;
            ret = hex_to_dec(hex);
        }
        else if (is.peek() == 'b' || is.peek() == 'B')
        {
            // 二进制
            is.get();
            string bin;
            is >> bin;
            ret = bin_to_dec(bin);
        }
        else if (isdigit(is.peek()))
        {
            // 十进制
            is >> ret;
        }
        else if (!isspace(is.peek()))
        {
            cerr << line_head() << "数字格式错误！\n";
            exit(1);
        }
    }
    else if (isdigit(is.peek()) || is.peek() == '-')
    {
        // 十进制
        is >> ret;
    }
    else
    {
        cerr << line_head() << "数字格式错误！\n";
        exit(1);
    }

    return ret;
}

string unsigned_dec_to_bin(unsigned int n)
{
    string r;
    while (n != 0)
    {
        r = (n % 2 == 0 ? "0" : "1") + r;
        n /= 2;
    }
    return r.empty() ? "0" : r;
}

string dec_to_bin(int n, int bin_size)
{
    string bin = unsigned_dec_to_bin(n >= 0 ? n : -n);
    if (n < 0)
    {
        for (char &ch : bin)
        {
            ch = ch == '0' ? '1' : '0';
        }
        int i = bin.size() - 1;
        while (i >= 0 && bin[i] == '1')
        {
            bin[i] = '0';
            --i;
        }
        if (i >= 0)
        {
            bin[i] = '1';
        }
        else
        {
            bin = "1" + bin;
        }

        if (bin.size() <= bin_size)
        {
            return string(bin_size - bin.size(), '1') + bin;
        }
        cerr << line_head() << "数字：" << n << "超出二进制位数：" << bin_size << "的限制！\n";
        exit(1);
    }
    else
    {
        if (bin.size() <= bin_size)
        {
            return string(bin_size - bin.size(), '0') + bin;
        }
        cerr << line_head() << "数字：" << n << "超出二进制位数：" << bin_size << "的限制！\n";
        exit(1);
    }
}

int bin_to_dec(const string &str)
{
    int res = 0;
    for (int i = 0; i < str.size(); ++i)
    {
        res = res * 2 + str[i] - '0';
    }
    return res;
}

int bin_to_signed_dec(const string &str)
{
    int res = str[0] == '1' ? -1 : 0;
    for (int i = 1; i < str.size(); ++i)
    {
        res = res * 2 + str[i] - '0';
    }
    return res;
}

string byte_to_hex(const string &byte)
{
    const vector vec{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    string ret;
    ret += vec[(byte[0] - '0') * 8 + (byte[1] - '0') * 4 + (byte[2] - '0') * 2 + (byte[3] - '0') * 1];
    ret += vec[(byte[4] - '0') * 8 + (byte[5] - '0') * 4 + (byte[6] - '0') * 2 + (byte[7] - '0') * 1];
    return ret;
}

int hex_to_dec(const string &hex)
{
    int res = 0;
    for (int i = 0; i < hex.size(); ++i)
    {
        if (hex[i] >= '0' && hex[i] <= '9')
            res = res * 16 + hex[i] - '0';
        else if (hex[i] >= 'A' && hex[i] <= 'F')
            res = res * 16 + hex[i] - 'A' + 10;
        else if (hex[i] >= 'a' && hex[i] <= 'f')
            res = res * 16 + hex[i] - 'a' + 10;
    }
    return res;
}

string dec_to_dword(unsigned int n)
{
    string res = "0x";
    string temp = dec_to_bin(n, 32);
    for (int i = 0; i < 4; ++i)
    {
        res += byte_to_hex(temp.substr(i * 8, 8));
    }
    return res;
}

string to_hex_line(const string &bit_line)
{
    string ret;
    for (int i = 0; i < 4; ++i)
    {
        ret += "0x" + byte_to_hex(bit_line.substr(i * 8, 8));
        if (i != 3)
            ret += " ";
    }
    return ret;
}

void pre_process(const char *filename)
{
    ifstream ifs(filename);
    string temp, symbol;
    char ch;
    pc = 0;
    while (!ifs.eof())
    {
        while (isspace(ifs.peek()))
            ifs.get();
        if (ifs.peek() == char_traits<char>::eof())
            break;

        if (ifs.peek() == '#')
        {
            getline(ifs, temp);
        }
        else if (ifs.peek() == '!')
        {
            ifs.get();
            ifs >> symbol;
            if (symbol == "define")
            {
                getline(ifs, temp);
            }
            else if (symbol == "label")
            {
                while (isspace(ifs.peek()))
                    ifs.get();

                if (ifs.get() != '.')
                {
                    cerr << line_head() << "标签格式错误！应以'.'为开头\n";
                    exit(1);
                }
                string label;
                ifs >> label;
                label_addr_map.insert({label, pc});
                addr_label_map.insert({pc, label});
            }
            else
            {
                cerr << line_head() << "未知宏命令\"" << symbol << "\"！\n";
                exit(1);
            }
        }
        else
        {
            ifs >> symbol;
            getline(ifs, temp);
            if (compound_type.count(symbol))
            {
                if (compound_type.count(symbol))
                {
                    pc += compound_type_size.at(symbol);
                }
                else
                {
                    cerr << line_head() << "未知指令\"" << symbol << "\"！\n";
                    exit(1);
                }
            }
            else
            {
                if (r_type.count(symbol) || i_type.count(symbol) || j_type.count(symbol))
                    pc += 1;
                else
                {
                    cerr << line_head() << "未知指令\"" << symbol << "\"！\n";
                    exit(1);
                }
            }
        }
        ++line;
    }
}

string line_head()
{
    return string("行") + to_string(line) + string("：");
}

string spaced_str(const string &str, int len, bool right_blank)
{
    if (str.size() >= len)
        return str;
    return right_blank ? str + string(len - str.size(), ' ') : string(len - str.size(), ' ') + str;
}