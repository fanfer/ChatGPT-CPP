#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "string"
#include <thread>
#include <mutex>
#include <iostream>
#include <fmt/core.h>
#include"cJSON.h"
#include <regex>
#include <fstream>
#include <sstream>
#include "base64.h" 
using namespace std;

struct MemoryStruct {
    char* memory;
    size_t size;
};


int string2flag(string s) {
    regex action_regex(R"(Action:\s*(\d+)\.)");
    smatch match;
    if (std::regex_search(s, match, action_regex)) {
        if (match.size() > 1) {
            std::string action_number = match[1].str();
            return stoi(action_number);
        }
    }
    return -1;
}

std::string replace_newlines(const std::string& str) {
    std::string result = str;
    std::string::size_type pos = 0;
    if (str.front() == '"' && str.back() == '"') {
        result = str.substr(1, str.size() - 2);
    }
    while ((pos = result.find("\\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos += 1;
    }
    return result;
}

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;
    char* ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);

    if (ptr == NULL) {
        printf("realloc() failed\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

std::string guess_mime_type(const std::string& image_path) {
    std::size_t dot_pos = image_path.find_last_of(".");
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    std::string extension = image_path.substr(dot_pos + 1);
    if (extension == "jpg" || extension == "jpeg") {
        return "image/jpeg";
    }
    else if (extension == "png") {
        return "image/png";
    }
    else if (extension == "gif") {
        return "image/gif";
    }
    else if (extension == "bmp") {
        return "image/bmp";
    }
    else if (extension == "tiff") {
        return "image/tiff";
    }
    else {
        return "application/octet-stream";
    }
}

std::string local_image_to_data_url(const std::string& image_path) {
    // Guess the MIME type of the image
    std::string mime_type = guess_mime_type(image_path);

    // Read the image file
    std::ifstream image_file(image_path, std::ios::binary);
    if (!image_file) {
        throw std::runtime_error("Could not open image file.");
    }
    std::ostringstream oss;
    oss << image_file.rdbuf();
    std::string image_data = oss.str();
    image_file.close();

    // Encode the image data to Base64
    std::string base64_encoded_data = base64_encode(reinterpret_cast<const unsigned char*>(image_data.c_str()), image_data.size());

    // Construct the data URL
    return "data:" + mime_type + ";base64," + base64_encoded_data;
}

char* SendJson(cJSON* json)
{
    CURL* curl;
    CURLcode res;
    struct MemoryStruct chunk = { 0 };
    char* printstr = cJSON_Print(json);
    char* result = NULL;
    char* answer = NULL;
    //printf("json data:%s\n", printstr);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        // 查看请求头
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        // 设置要请求的 URL
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.zhizengzeng.com/v1/chat/completions");

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

        // 设置请求头信息
        curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Authorization: Bearer api-key");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        // Authorization: Bearer $OPENAI_API_KEY

        // 添加 POST 数据
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, printstr);

        // 执行请求
        res = curl_easy_perform(curl);  // 请求成功这里会打印消息

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }
        else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            //printf("HTTP response code: %ld\n", http_code);
            //printf("chunk memory:%s\n", chunk.memory);
            cJSON* response = cJSON_Parse(chunk.memory);
            cJSON* choices = cJSON_GetObjectItem(response, "choices");
            cJSON* Msg = cJSON_GetArrayItem(choices, 0);
            cJSON* item_0 = cJSON_GetObjectItem(Msg, "message");
            cJSON* item_1 = cJSON_GetObjectItem(item_0, "content");
            result = cJSON_Print(item_0);
            answer = cJSON_Print(item_1);
            //int len = MultiByteToWideChar(CP_UTF8, 0, item_1->valuestring, -1, NULL, 0);
            //wchar_t* transform = new wchar_t[len];
            //MultiByteToWideChar(CP_UTF8, 0, item_1->valuestring, -1, transform, len);
            //len = WideCharToMultiByte(CP_ACP, 0, transform, -1, NULL, 0, NULL, NULL);
            ////show = new char[len];
            //WideCharToMultiByte(CP_ACP, 0, transform, -1, show, len, NULL, NULL);
            //printf("answer:%s\n", show);
            //delete[] transform;

            cJSON_Delete(response);
        }

        // 清理 curl
        curl_easy_cleanup(curl);
    }
    if (chunk.memory != NULL)
        free(chunk.memory);
    if (printstr != NULL)
        free(printstr);
    return answer;
}

//gpt-4-vision-preview
char* capteImage(string url, string text = "what's in this image?") {
    cJSON* json = cJSON_Parse("{\"model\": \"gpt-4-vision-preview\"}");
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "messages", array);

    //system prompt
    string sysPrompt = "You are a photo interpretation assistant. Please provide a detailed description of the lake surface in the captured image. You must answer 2 questions below:\nWhether there are any buildings or obstacles on the lake surface.\nWhether there are any boats on the lake surface. Be concise, up to 50 words.";
    cJSON* sysitem = cJSON_CreateObject();
    cJSON_AddStringToObject(sysitem, "role", "system");
    cJSON_AddStringToObject(sysitem, "content", sysPrompt.c_str());
    cJSON_AddItemToArray(array, sysitem);
    //user
    cJSON* item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "role", "user");
    cJSON* content = cJSON_CreateArray();
    cJSON* content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", text.c_str());
    cJSON* content_item2 = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item2, "type", "image_url");
    cJSON* url_item = cJSON_CreateObject();
    cJSON_AddStringToObject(url_item, "url", url.c_str());
    cJSON_AddItemToObject(content_item2, "image_url", url_item);
    cJSON_AddItemToArray(content, content_item);
    cJSON_AddItemToArray(content, content_item2);
    cJSON_AddItemToObject(item, "content", content);
    //cJSON_AddStringToObject(item, "content", buf);
    cJSON_AddItemToArray(array, item);
    //printf(cJSON_Print(json));
    char* answer = SendJson(json);
    //if (answer != NULL)
    //{
    //    //printf("answer json:%s\n", answer);
    //    cJSON_AddItemToArray(array, cJSON_Parse(answer));
    //}
    //else
    //{
    //    cJSON_DeleteItemFromArray(item, cJSON_GetArraySize(array) - 1);
    //}
    cout<< "\bImage Description:" << endl;
    cout << replace_newlines(answer) << endl<<endl;
    cJSON_Delete(json);
    return answer;
}

char* decideFormation(const string& envDescripstion, const string& user_input) {
    cJSON* json = cJSON_Parse("{\"model\": \"gpt-4o\"}");
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "messages", array);
    //system prompt
    string sysPrompt = "Assuming you are a fleet commander, you need to judge the environment in which the fleet is located based on the information from your superiors and the formation information sent back by the front fleet, and then decide on the fleet's formation strategy. There are four types of environments:\n0: Open space with no target ships, execute horizontal formation;\n1: Open space with target ships, execute encirclement formation;\n2: Narrow (such as rivers, ports, or with obstacles like bridges, lighthouses, etc.) with no target ships, execute vertical formation;\n3: Narrow(such as rivers, ports, or with obstacles like bridges, lighthouses, etc.) with target ships, execute wait in place; \nPriority is given to executing the superior's orders, fully trusting the supplementary information from the superior.\nAnswer according to the template below:\n Thought: How to analyze and obtain the result. \nAction: Decision instruction(0, 1, 2, or 3).\n For example:\nThought:There is a bridge over the water without boats, so it is class 2. Action:2.";
    cJSON* sysitem = cJSON_CreateObject();
    cJSON_AddStringToObject(sysitem, "role", "system");
    cJSON_AddStringToObject(sysitem, "content", sysPrompt.c_str());
    cJSON_AddItemToArray(array, sysitem);
    //user
    string text = fmt::format("The environment of the water surface ahead is:{} \nThe superior's order is: {}", envDescripstion, user_input);
    cJSON* item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "role", "user");
    cJSON_AddStringToObject(item, "content", text.c_str());
    cJSON_AddItemToArray(array, item);
    char* answer = SendJson(json);
    string s(answer);
    cJSON_Delete(json);
    int flag = string2flag(s);
    cout <<replace_newlines(answer) << endl;
    cout << "===================================" << endl;
    return answer;
    

}

// 全局变量和互斥锁
string text;
string envDescription;
mutex mtx;
int flag = -1;
mutex mtx_flag;
int gpt_modified = 0;

//thread 1 ：维护字符串信息 envDes
void generateDes() {
    while (true) {
        string img_url;
        //TODO: 从摄像头获取图片
        try {
            std::string image_path = "C:/Users/admin/source/repos/chatcpp/images/2.png";  // Replace with your image path
            std::string data_url = local_image_to_data_url(image_path);
            img_url = data_url;
        }
        catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
       //envDescription = capteImage("https://picture.fanfer.top/Test_pics/IMG_5022.JPG");//换成image_URL
        envDescription = capteImage(img_url);
        string user_input = "No any orders.";
        char* return_flag = decideFormation(envDescription, user_input);
        if (gpt_modified < 2) {
            lock_guard<mutex> lock2(mtx_flag);
            //TODO: 从return_flag中提取出来的信息判断是不是需要修改
            flag = 1;
            gpt_modified++;
        }
        continue;
    }
}

// /thread 2 ，监控用户输入的线程函数
void monitor_input() {
    string user_input;
    while (true) {
        getline(cin, user_input);
        lock_guard<mutex> lock(mtx);
        char* return_flag = decideFormation(envDescription, user_input);
        lock_guard<mutex> lock2(mtx_flag);
        flag = 1;
    }
}

int main(int argc, char* argv[])
{
    // 创建线程
    thread thread_a(generateDes);
    thread thread_b(monitor_input);

    // 等待线程完成（实际上，这个程序是无限循环的，主线程会一直等待）
    thread_a.join();
    thread_b.join();

    return 0;

}
