#pragma once

#include <iostream>
#include <mutex>
#include <stdexcept>
#include "micro_thread_pool.hpp"
#include "plugin.hpp"
#include <condition_variable>
#include <atomic>
#include <list>
#include <map>
#include <string>
#include <shared_mutex>
#include <chrono>
#include <thread>



#define MICRO_KERNEL_VERSION "1.0.0"

template <typename T>
class MicroKernel : public IMicroKernelServices<T> {
public:
    MicroKernel(uint32_t plugin_limit, std::shared_ptr<IThreadPool> thread_pool)
        : version_(MICRO_KERNEL_VERSION),
        limit_(plugin_limit),
        thread_pool_(thread_pool),
        running_(false),
        exit_(false) {
        if (!thread_pool_) {
            throw std::invalid_argument("thread_pool is null");
        }
    }

    virtual ~MicroKernel() { stop(); }

    void run(void) {
        {
            std::unique_lock<std::shared_mutex> lck(mtx_);

            if (running_) {
                return;
            }

            std::list<PluginKey<T>> bad_plugin;

            for (auto& kv : plugins_) {
                if (kv.second) {
                    kv.second->set_micro_kernel_srv(this);
                    if (!kv.second->plugin_init()) {
                        kv.second->set_plugin_status(E_PLUGIN_BAD);
                        std::cout << "plugin : [name = " << kv.first.name
                            << "] [version = " << kv.first.version
                            << "] init failed" << std::endl;
                        bad_plugin.push_front(kv.first);
                    }
                }
            }

            for (auto& item : bad_plugin) {
                auto it = plugins_.find(item);
                if (it != plugins_.end()) {
                    plugins_.erase(it);
                }
            }
            bad_plugin.clear();

            for (auto& kv : plugins_) {
                if (kv.second) {
                    if (!kv.second->plugin_start()) {
                        kv.second->set_plugin_status(E_PLUGIN_BAD);
                        std::cout << "plugin : [name = " << kv.first.name
                            << "] [version = " << kv.first.version
                            << "] start failed" << std::endl;
                        bad_plugin.push_front(kv.first);
                    }
                    else {
                        kv.second->set_plugin_status(E_PLUGIN_RUNING);
                    }
                }
            }

            for (auto& item : bad_plugin) {
                auto it = plugins_.find(item);
                if (it != plugins_.end()) {
                    plugins_.erase(it);
                }
            }
            bad_plugin.clear();

            running_ = true;
            exit_ = false;
        }

        while (running_) {
            {
                std::shared_lock<std::shared_mutex> lck(mtx_);

                for (auto& kv : plugins_) {
                    if (!running_) {
                        break;
                    }
                    if (kv.second->plugin_task_en()) {
                        thread_pool_->add_task([plug = kv.second] {
                            plug->plugin_task();
                            });
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        {
            std::unique_lock<std::shared_mutex> lck(mtx_);
            exit_ = true;
            micro_kernel_exited_.notify_one();
        }
    }

private:
    void stop(void) {
        std::unique_lock<std::shared_mutex> lck(mtx_);

        if (!running_) {
            return;
        }

        running_ = false;

        micro_kernel_exited_.wait(lck, [this] { return exit_; });

        for (auto& kv : plugins_) {
            if (E_PLUGIN_RUNING == kv.second->plugin_status()) {
                kv.second->plugin_stop();
                kv.second->plugin_exit();
                kv.second->set_plugin_status(E_PLUGIN_STOP);
            }
        }
    }

public:
    virtual std::string micro_kernel_version(void) override {
        return version_;
    }

    virtual uint32_t plugin_cnt(void) override {
        std::shared_lock<std::shared_mutex> lck(mtx_);
        return static_cast<uint32_t>(plugins_.size());
    }

    virtual bool plugin_key(const T& key, PluginKey<T>& item_key) override {
        std::shared_lock<std::shared_mutex> lck(mtx_);

        PluginKey<T> tmp;
        tmp.key = key;

        auto it = plugins_.find(tmp);
        if (it == plugins_.end()) {
            return false;
        }
        item_key = it->first;
        return true;
    }

    virtual bool message_dispatch(const PluginKey<T>& from, const T& to_key,
        const PluginDataT& request,
        PluginDataT& response) override {
        std::shared_lock<std::shared_mutex> lck(mtx_);

        PluginKey<T> to;
        to.key = to_key;

        auto it = plugins_.find(to);
        if (it == plugins_.end()) {
            return false;
        }

        to = it->first;
        auto plugin = it->second;

        lck.unlock();

        const PluginMessage<T> req_msg{ from, to, request };
        PluginMessage<T> res_msg{ to, from, response };

        bool ret = plugin->message(req_msg, res_msg);
        response = res_msg.data;
        return ret;
    }

    virtual bool stream_dispatch(std::shared_ptr<IPluginStream<T>> stream) override {
        std::shared_lock<std::shared_mutex> lck(mtx_);

        auto it = plugins_.find(stream->to_);
        if (it == plugins_.end()) {
            return false;
        }
        stream->to_.name = it->first.name;
        stream->to_.version = it->first.version;
        auto plugin = it->second;
        thread_pool_->add_task([=] {
            plugin->stream(stream);
            });
        return true;
    }

    virtual void log(const std::string& message) override {
        std::cout << "[MicroKernel LOG] " << message << std::endl;
    }

    bool plugin_register(std::shared_ptr<IPlugin<T>> plugin) {
        std::unique_lock<std::shared_mutex> lck(mtx_);
        if (plugins_.size() >= limit_) {
            return false;
        }

        auto item = plugins_.find(plugin->plugin_key_);
        if (item != plugins_.end()) {
            return false;
        }

        plugin->set_micro_kernel_srv(this);
        if (running_) {
            if (!plugin->plugin_init() || !plugin->plugin_start()) {
                plugin->set_micro_kernel_srv(nullptr);
                return false;
            }
            plugin->set_plugin_status(E_PLUGIN_RUNING);
        }

        plugins_.insert(std::make_pair(plugin->plugin_key_, plugin));
        return true;
    }

    bool plugin_unregister(const T& key) {
        std::unique_lock<std::shared_mutex> lck(mtx_);

        PluginKey<T> tmp;
        tmp.key = key;

        auto it = plugins_.find(tmp);
        if (it == plugins_.end()) {
            return false;
        }
        if (running_ && it->second->plugin_status() == E_PLUGIN_RUNING) {
            it->second->plugin_stop();
            it->second->plugin_exit();
            it->second->set_plugin_status(E_PLUGIN_STOP);
        }

        plugins_.erase(it);
        return true;
    }

private:
    std::shared_mutex mtx_;
    std::string version_;
    uint32_t limit_;
    std::map<PluginKey<T>, std::shared_ptr<IPlugin<T>>> plugins_;
    std::shared_ptr<IThreadPool> thread_pool_;
    std::condition_variable_any micro_kernel_exited_;  
    std::atomic_bool running_;
    bool exit_;

};

