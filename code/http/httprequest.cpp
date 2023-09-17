/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 
#include "httprequest.h"
using namespace std;

// TODO：将HTTP和Mysql查询结合在一起耦合性太高了，几乎没有拓展性

// 设置页面路径
const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/login", "/register", "/index" ,"/error" ,"/JSON",
            "/linux", 
            "/Xshell",
            "/Docker2022",
            "/lucky"};
// 设置页面类型映射
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

// 判断HTTP是否为长连接
bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        // 当连接里含有keep-alive并且为1.1版本时，返回true
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    // 没有连接直接返回flase
    return false;
}
// 从缓冲区内解析HTTp请求报文
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    // 判断是否有信息
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) {
        // 这行代码通过搜索 buff 中的数据，查找HTTP请求头的结束位置，也就是找到第一个出现的 "\r\n\r\n"，并将其指针保存在 lineEnd 变量中。
        // 当搜索到一个完整的HTTP换行符时，指针向后移动2个字符，以便于继续解析下一行。
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        // 读取HTTP请求头的信息
        std::string line(buff.Peek(), lineEnd);
        switch(state_)
        {
        //  解析HTTP请求行，包括请求方法、请求路径和HTTP版本
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;   
        // 解析HTTP请求头部字段。 
        case HEADERS:
            ParseHeader_(line);
            // 跳过两个换行符
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        // 这部分代码可能是处理POST请求的数据，根据具体情况进行解析。
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) { break; }
        // 清除两个换行符
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}
// 解析请求路径
void HttpRequest::ParsePath_() {
    // 请求为空时，默认返回index
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        //逐个去匹配路径
        // TODO：是否应该加上未找到返回的404页面？ 
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}
// GET /index.html HTTP/1.1
// regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");定义了一个正则表达式模式 pattern，用于匹配 HTTP 请求行的格式
// ^：匹配行的起始位置。
// ([^ ]*)：匹配一个或多个非空格字符，这里用括号捕获了匹配结果，分别代表请求方法、请求路径和协议版本。
// （空格）：匹配一个空格字符，用于分隔不同部分。
// HTTP/：匹配文本字符串 "HTTP/"。
// ([^ ]*)：再次匹配一个或多个非空格字符，用于捕获协议版本。
// $：匹配行的结束位置。
// 解析HTTP请求行 请求方法 ，请求路径和协议版本
bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    // 用于存储正则表达式匹配的子串结果。C11
    smatch subMatch;
    // 尝试匹配输入的 line 字符串和正则表达式模式 pattern。如果匹配成功，就会将匹配结果存储在 subMatch 中。
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}
// ^：匹配行的起始位置。
// ([^:]*)：匹配零个或多个非冒号字符，用于捕获键名（即头部字段的名称）。
// :：匹配一个冒号字符，用于分隔键名和键值。
// ?：匹配零个或一个空格字符，允许键值之间有可选的空格。
// (.*)：匹配零个或多个字符，用于捕获键值（即头部字段的值）。
// $：匹配行的结束位置。
// 解析HTTP请求头
void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
   
    // Host: www.example.com
    // User-Agent: Mozilla/5.0
    // Accept-Language: en-US,en;q=0.5
    // Content-Type: application/json

    // "Host" 对应的值为 "www.example.com"
    // 将匹配结果中第一个括号捕获的内容（即键名）作为键，将第二个括号捕获的内容（即键值）作为值，存储在 header_ 中。
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;
    }
}
// 解析请求体（POST请求数据）
void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}
// 将十六进制转化为整数
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}
// 解析POST请求表单数据
void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        // 解析Post请求
        ParseFromUrlencoded_();
        // 查找是否存在路径
        if(DEFAULT_HTML_TAG.count(path_)) {
            // 返回路径网页
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            // 路径是注册页面（tag为0）或登录页面（tag为1）
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    // key（用于存储表单字段名）、value（用于存储表单字段值）、num（用于存储解析的十六进制数值）、n（请求体长度）、i 和 j（循环变量）。
    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    // 示例name=John+Doe&age=30&city=New+York
    // 键: "name", 值: "John Doe"
    // 键: "age", 值: "30"
    // 键: "city", 值: "New York"

    // 原始文本：你好
    // 编码后：%E4%BD%A0%E5%A5%BD
    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            // 从 j 到 i-1 的部分就是字段名
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            // +替换为空格，以正确还原 URL 编码的空格字符
            body_[i] = ' ';
            break;
        case '%':
            // 它会解析出十六进制数值，并将 % 后面的两个字符转换成一个字符，然后更新 i 为 i+2
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            // 可以将数字值转换为对应的ASCII字符
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            // 它会解析出字段值（value），从 j 到 i-1 的部分就是字段值。然后，它将字段名和字段值存储在 post_ 中，并打印调试信息
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    // 当遇到 & 符号时，表示一个字段的解析结束，字段名和字段值被存储在 post_ 中。
    // 但是，如果请求体的最后一个字段后面没有 & 符号，那么最后一个字段的值就不会被存储，因为循环结束了。
    // 确保即使最后一个字段后没有 & 符号，最后一个字段的值仍然能够被正确解析和存储在 post_ 中。
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}
// 用于验证用户信息，并且确认mysql的查询
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    // 不为空
    assert(sql);
    
    bool flag = false;
    // 用于记录字段数量
    unsigned int j = 0;
    char order[256] = { 0 };
    // 存储查询结果的字段信息
    MYSQL_FIELD *fields = nullptr;
    // 存储查询结果
    MYSQL_RES *res = nullptr;
    
    // 如果不是登录操作（即注册操作），则将 flag 设置为 true，表示注册行为（因为不是登录，所以默认为注册）
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 使用 MySQL API 执行 SQL 查询，如果查询失败，则释放结果集资源并返回 false
    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    // 遍历查询结果集中的每一行，获取用户名和密码字段的值。
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

// 用于获取 POST 请求中的表单数据，传入键名，返回对应的值
std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}
// 重载版本，接受一个字符指针作为键名
std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}