#include "HttpServer.h"
#include "cpp-httplib/httplib.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "Memory.h"
#include "Utils.h"

#ifdef WITH_PIPER
#include "TtsPiper.h"
#endif

#ifdef WITH_AUDIO
#include "Audio.h"
#endif

#ifdef WITH_VOSK
#include "AsrVosk.h"
#endif

using json = nlohmann::json;

// --- PIMPL Idiom ---
struct HttpServer::Impl {
    HttpOpts opts;
    httplib::Server svr;
    std::thread server_thread;
    bool running = false;
    MemoryStore* mem = nullptr; // Non-owning pointer to the memory store from main

    // Pointers to other main components (non-owning)
    // These are set after construction via setters to avoid huge constructors
    TtsPiper* tts = nullptr;
    AsrVosk* asr = nullptr;
    Audio* audio = nullptr;

    void register_routes();
    void ws_broadcast(const std::string& msg);
};

// --- Public API ---

HttpServer::HttpServer(const HttpOpts& opts) : impl_(new Impl{opts}) {
    // defer route registration until components are set
}

HttpServer::~HttpServer() {
    stop();
    delete impl_;
}

bool HttpServer::start() {
    if (isRunning()) {
        return false;
    }
    // Need to get access to the main components. For now, let's assume they are passed somehow
    // For the purpose of this example, we'll just new them up.
    // In a real app, you'd pass them in from main.
    impl_->mem = new MemoryStore();
    impl_->mem->load();

    impl_->register_routes();

    try {
        impl_->server_thread = std::thread([this]() {
            std::cout << "[http] Server starting on " << impl_->opts.host << ":" << impl_->opts.port << std::endl;
            if (!impl_->svr.listen(impl_->opts.host.c_str(), impl_->opts.port)) {
                std::cerr << "[http] Server failed to start." << std::endl;
                impl_->running = false;
            }
        });
        // Brief pause to let the server thread initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        impl_->running = true;
    } catch (const std::exception& e) {
        std::cerr << "[http] Exception on server start: " << e.what() << std::endl;
        return false;
    }
    return true;
}

void HttpServer::stop() {
    if (!isRunning()) {
        return;
    }
    impl_->svr.stop();
    if (impl_->server_thread.joinable()) {
        impl_->server_thread.join();
    }
    impl_->running = false;
    std::cout << "[http] Server stopped." << std::endl;
}

bool HttpServer::isRunning() const {
    return impl_->running && impl_->svr.is_running();
}

void HttpServer::pushEvent(const std::string& jsonLine) {
    if (isRunning()) {
        impl_->ws_broadcast(jsonLine);
    }
}


// --- Private Impl ---

void HttpServer::Impl::register_routes() {
    // Middleware for bearer token authentication
    auto auth_middleware = [this](const httplib::Request& req, httplib::Response& res) {
        if (!opts.bearer.empty()) {
            if (!req.has_header("Authorization")) {
                res.status = 401;
                res.set_content("{\"error\":\"Authorization header missing\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
            std::string auth_header = req.get_header_value("Authorization");
            std::string prefix = "Bearer ";
            if (auth_header.rfind(prefix, 0) != 0) {
                res.status = 401;
                res.set_content("{\"error\":\"Invalid token format\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
            std::string token = auth_header.substr(prefix.length());
            if (token != opts.bearer) {
                res.status = 401;
                res.set_content("{\"error\":\"Invalid token\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    };
    svr.set_pre_routing_handler(auth_middleware);

    // --- System Endpoints ---
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        json r = {{"status", "ok"}};
        res.set_content(r.dump(), "application/json");
    });

    svr.Get("/api/version", [](const httplib::Request&, httplib::Response& res) {
        // In a real app, this would come from a build-time variable
        json r = {{"version", "0.1.0-dev"}};
        res.set_content(r.dump(), "application/json");
    });

    // --- Memory Endpoints ---
    svr.Get("/api/memory/facts", [this](const httplib::Request&, httplib::Response& res) {
        auto facts = mem->listFacts();
        json r = {{"facts", json::array()}};
        for(const auto& p : facts) {
            r["facts"].push_back({{"key", p.first}, {"value", p.second}});
        }
        res.set_content(r.dump(), "application/json");
    });

    svr.Post("/api/memory/set", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string key = body.at("key");
            std::string value = body.at("value");
            mem->set(key, value);
            mem->save();
            res.set_content("{\"ok\":true}", "application/json");
        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    svr.Post("/api/memory/get", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string key = body.at("key");
            std::string value;
            if (mem->get(key, value)) {
                res.set_content("{\"ok\":true, \"value\":\"" + value + "\"}", "application/json");
            } else {
                res.set_content("{\"ok\":false}", "application/json");
            }
        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    svr.Post("/api/memory/del", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string key = body.at("key");
            mem->del(key);
            mem->save();
            res.set_content("{\"ok\":true}", "application/json");
        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // --- Note Endpoints ---
    svr.Post("/api/note/add", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string text = body.at("text");
            std::string id = mem->addNote(text);
            mem->save();
            res.set_content("{\"ok\":true, \"id\":\"" + id + "\"}", "application/json");
        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    svr.Get("/api/note/list", [this](const httplib::Request&, httplib::Response& res) {
        auto notes = mem->listNotes();
        json r = {{"notes", json::array()}};
        for(const auto& n : notes) {
            r["notes"].push_back({{"id", n.id}, {"text", n.text}, {"created_at", n.created_at}});
        }
        res.set_content(r.dump(2), "application/json");
    });

    svr.Post("/api/note/del", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string id = body.at("id");
            if (mem->deleteNote(id)) {
                mem->save();
                res.set_content("{\"ok\":true}", "application/json");
            } else {
                res.status = 404;
                res.set_content("{\"ok\":false, \"error\":\"Note not found\"}", "application/json");
            }
        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // --- TTS Endpoint ---
    svr.Post("/api/tts/say", [this](const httplib::Request& req, httplib::Response& res){
        json r;
#ifdef WITH_PIPER
        if (!tts || !tts->isAvailable()) {
            r = {{"ok", false}, {"error", "TTS not available"}};
            res.status = 503;
            res.set_content(r.dump(), "application/json");
            return;
        }
        try {
            json body = json::parse(req.body);
            std::string text = body.at("text");

            double sample_rate = 0;
            std::vector<int16_t> pcm = tts->synthesize(text, sample_rate);

            if (pcm.empty()) {
                r = {{"ok", false}, {"error", "TTS synthesis failed"}};
                res.status = 500;
            } else {
                r = {{"ok", true}, {"played", false}, {"saved_path", nullptr}};
#ifdef WITH_AUDIO
                if (audio) {
                    audio->playback(-1, sample_rate, pcm);
                    r["played"] = true;
                }
#endif
            }
            res.set_content(r.dump(), "application/json");

        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
#else
        r = {{"ok", false}, {"error", "TTS support not compiled (WITH_PIPER=OFF)"}};
        res.status = 501;
        res.set_content(r.dump(), "application/json");
#endif
    });

    // --- ASR Endpoint ---
    svr.Post("/api/asr/ptt_once", [this](const httplib::Request& req, httplib::Response& res){
        json r;
#if defined(WITH_AUDIO) && defined(WITH_VOSK)
        if (!asr || !asr->isAvailable() || !audio) {
            r = {{"ok", false}, {"error", "Audio/ASR not available"}};
            res.status = 503;
            res.set_content(r.dump(), "application/json");
            return;
        }
        try {
            json body = json::parse(req.body);
            int seconds = body.value("seconds", 10);

            std::vector<int16_t> pcm_data;
            double sample_rate = 16000.0;
            audio->recordPtt(-1, seconds, sample_rate, pcm_data);

            if (pcm_data.empty()) {
                r = {{"ok", false}, {"error", "No audio recorded"}};
            } else {
                std::string transcript = asr->transcribe(pcm_data, sample_rate);
                r = {{"ok", true}, {"transcript", transcript}};
            }
            res.set_content(r.dump(), "application/json");

        } catch (json::exception& e) {
            res.status = 400;
            res.set_content("{\"ok\":false, \"error\":\"Invalid JSON\"}", "application/json");
        }
#else
        r = {{"ok", false}, {"error", "Audio/ASR support not compiled (WITH_AUDIO=OFF or WITH_VOSK=OFF)"}};
        res.status = 501;
        res.set_content(r.dump(), "application/json");
#endif
    });

    // --- WebSocket Endpoint ---
    if (opts.enable_ws) {
        svr.Get("/ws/events", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.is_upgrade_request()) {
                res.set_upgrade_handler([this](httplib::Stream& stream) {
                    std::cout << "[ws] Client connected." << std::endl;
                    // Send hello message
                    json hello = {{"type", "hello"}, {"version", "0.1.0-dev"}};
                    stream.send(hello.dump());

                    // Keep connection open, but we send data via broadcast
                    while(svr.is_running()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }
                    std::cout << "[ws] Client disconnected." << std::endl;
                });
            } else {
                res.status = 400;
                res.set_content("Not a WebSocket upgrade request", "text/plain");
            }
        });
    }
}

void HttpServer::Impl::ws_broadcast(const std::string& msg) {
    if (opts.enable_ws) {
        svr.send_to_all_websocket_connections(msg);
    }
}
