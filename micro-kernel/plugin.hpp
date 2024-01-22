#pragma once

#include <time.h>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>


template <typename T>
class IPlugin;
template <typename T>
class MicroKernel;

template <typename T>
struct PluginKey {
    PluginKey() {}
    PluginKey(const std::string& name, const std::string& version, const T& key)
        : name(name), version(version), key(key) {
    }
    PluginKey(const PluginKey<T>& key)
        : name(key.name), version(key.version), key(key.key) {
    }

    bool operator==(const PluginKey<T>& keyn) const { return key == keyn.key; }
    bool operator>(const PluginKey<T>& keyn) const { return key > keyn.key; }
    bool operator<(const PluginKey<T>& keyn) const { return key < keyn.key; }

    std::string name;
    std::string version;
    T key;
};

struct PluginDataT {
    int type;
    int len;
    void* data;
};

template <typename T>
struct PluginMessage {
    PluginKey<T> from;
    PluginKey<T> to;
    PluginDataT data;
};

template <typename T>
class IPluginStream {
public:
    IPluginStream(const PluginKey<T>& from, const PluginKey<T>& to)
        : from_(from), to_(to) {
    }
    virtual ~IPluginStream() {}

    virtual void close() = 0;
    virtual bool is_closed(void) = 0;
    virtual int send(const PluginDataT& data, const time_t wait = -1) = 0;
    virtual int recv(PluginDataT& data, const time_t wait = -1) = 0;

public:
    PluginKey<T> from_;
    PluginKey<T> to_;
};

typedef enum {
    E_PLUGIN_STOP = 0,
    E_PLUGIN_RUNING = 1,
    E_PLUGIN_BAD = 2,
} plugin_run_status;

template <typename T>
class IMicroKernelServices {
public:
    virtual ~IMicroKernelServices() {}

    virtual std::string micro_kernel_version(void) = 0;
    virtual uint32_t plugin_cnt(void) = 0;
    virtual bool plugin_key(const T& key, PluginKey<T>& item_key) = 0;
    virtual bool message_dispatch(const PluginKey<T>& from, const T& to_key,
        const PluginDataT& request,
        PluginDataT& response) = 0;
    virtual bool stream_dispatch(std::shared_ptr<IPluginStream<T>> stream) = 0;

    virtual void log(const std::string& message) = 0;
};

template <typename T>
class IPlugin {
public:
    IPlugin(const PluginKey<T>& key)
        : plugin_key_(key),
        plugin_st_(E_PLUGIN_STOP),
        mic_kernel_srv_(nullptr) {
    }

    virtual ~IPlugin() {}

    const PluginKey<T>& plugin_key(void) const { return plugin_key_; }

    plugin_run_status plugin_status(void) { return plugin_st_; }

    IMicroKernelServices<T>* get_micro_kernel_service(void) {
        return mic_kernel_srv_;
    }

    bool continue_run(void) {
        if (plugin_st_ == E_PLUGIN_STOP) {
            if (!plugin_start()) {
                return false;
            }
            plugin_st_ = E_PLUGIN_RUNING;
            return true;
        }
        return false;
    }

    bool pause_run(void) {
        if (plugin_st_ == E_PLUGIN_RUNING) {
            if (!plugin_stop()) {
                return false;
            }
            plugin_st_ = E_PLUGIN_STOP;
            return true;
        }
        return false;
    }

    bool register_self(IMicroKernelServices<T>* micro_kernel) {
        if (mic_kernel_srv_) {
            return false;
        }
        if (!micro_kernel) {
            return false;
        }

        auto mk = dynamic_cast<MicroKernel<T>*>(micro_kernel);
        if (!mk) {
            return false;
        }
        bool ret = mk->plugin_register(std::shared_ptr<IPlugin<T>>(this));
        if (!ret) {
            return false;
        }
        mic_kernel_srv_ = micro_kernel;
        return true;
    }

    bool unregister_self(void) {
        if (!mic_kernel_srv_) {
            return false;
        }

        if (plugin_st_ == E_PLUGIN_RUNING) {
            plugin_stop();
            plugin_exit();
            plugin_st_ = E_PLUGIN_STOP;
        }

        auto mk = dynamic_cast<MicroKernel<T>*>(mic_kernel_srv_);
        if (mk) {
            mk->plugin_unregister(plugin_key_.key);
        }

        mic_kernel_srv_ = nullptr;
        return true;
    }

public:
    virtual bool plugin_init(void) = 0;
    virtual bool plugin_start(void) = 0;
    virtual bool plugin_task(void) = 0;
    virtual bool plugin_task_en(void) = 0;
    virtual bool plugin_stop(void) = 0;
    virtual bool plugin_exit(void) = 0;
    virtual bool notice(const PluginDataT& msg) = 0;
    virtual bool message(const PluginMessage<T>& request,
        PluginMessage<T>& response) = 0;
    virtual bool stream(std::shared_ptr<IPluginStream<T>> stream) = 0;

private:
    friend class MicroKernel<T>;
    void set_plugin_status(plugin_run_status st) { plugin_st_ = st; }
    void set_micro_kernel_srv(IMicroKernelServices<T>* mic_kernel_srv) {
        mic_kernel_srv_ = mic_kernel_srv;
    }

private:
    PluginKey<T> plugin_key_;
    plugin_run_status plugin_st_;
    IMicroKernelServices<T>* mic_kernel_srv_;
};

