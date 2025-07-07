#pragma once
#include "../core/controller.hpp"
#include <sstream>

class TestBodyController : public CORE::Controller {
public:
    void handle(const CORE::Request& req, CORE::Response& res) override {
        res.status_code = 200;
        res.status_text = "OK";
        res.headers["Content-Type"] = "application/json";
        
        // Build JSON response showing what we parsed
        std::ostringstream json;
        json << "{\n";
        json << "  \"method\": \"" << req.method << "\",\n";
        json << "  \"path\": \"" << req.path << "\",\n";
        json << "  \"body_type\": \"" << body_type_to_string(req.parsed_body.type) << "\",\n";
        json << "  \"parsing_success\": " << (req.parsed_body.success ? "true" : "false") << ",\n";
        
        if (!req.parsed_body.success) {
            json << "  \"error\": \"" << req.parsed_body.error_message << "\",\n";
        }
        
        json << "  \"raw_body_size\": " << req.body.size() << ",\n";
        
        // Show parsed content based on type
        if (req.parsed_body.type == CORE::BodyType::JSON) {
            json << "  \"json_content\": \"" << escape_json(req.parsed_body.json_string) << "\",\n";
        } else if (req.parsed_body.type == CORE::BodyType::FORM_URLENCODED) {
            json << "  \"form_data\": {\n";
            bool first = true;
            for (const auto& [key, value] : req.parsed_body.form_data) {
                if (!first) json << ",\n";
                json << "    \"" << escape_json(key) << "\": \"" << escape_json(value) << "\"";
                first = false;
            }
            json << "\n  },\n";
        }
        
        // Show some headers
        json << "  \"content_type\": \"";
        auto ct = req.headers.find("content-type");
        if (ct != req.headers.end()) {
            json << escape_json(ct->second);
        }
        json << "\",\n";
        
        json << "  \"content_length\": \"";
        auto cl = req.headers.find("content-length");
        if (cl != req.headers.end()) {
            json << cl->second;
        }
        json << "\"\n";
        
        json << "}";
        
        res.body = json.str();
        res.headers["Content-Length"] = std::to_string(res.body.size());
    }
    
private:
    std::string body_type_to_string(CORE::BodyType type) {
        switch (type) {
            case CORE::BodyType::NONE: return "none";
            case CORE::BodyType::JSON: return "json";
            case CORE::BodyType::FORM_URLENCODED: return "form_urlencoded";
            case CORE::BodyType::MULTIPART: return "multipart";
            case CORE::BodyType::RAW: return "raw";
            default: return "unknown";
        }
    }
    
    std::string escape_json(const std::string& str) {
        std::string escaped;
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }
};