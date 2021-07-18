// USED HEADERS
#include "json.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <memory>
#include <esp_log.h>
#include <esp8266/eagle_soc.h>

// CFG HEADERS (for variable decls)
#include "/home/matthew/SignCode/esp/main/grabber/ttc.cfg.h"
#include "/home/matthew/SignCode/esp/main/grabber/multi.cfg.h"

#pragma GCC diagnostic ignored "-Wbraced-scalar-init"
#pragma GCC diagnostic ignored "-Wunused-label"
const static char * TAG = "configparse";

// VARIABLE DECLARATIONS (from other files)

char const * ttc::alert_search{nullptr};
ttc::TTCEntry ttc::entries[3] = {{}, {}, {}};
uint8_t ttc::number_of_entries{0};
multi::BlockThing multi::bt{};

namespace config {
    // STATE ENUM
    enum struct cfgstate : uint16_t {
        R,
        R__TTC,
        R__TTC__ENTRIES_ALL,
        R__BLOCK,
    };
    
    // EXPLICIT DEFAULT
    void clear_config() {
        { if (IS_DRAM(::ttc::alert_search) || IS_IRAM(::ttc::alert_search)) free(const_cast<char *>(::ttc::alert_search));
        ::ttc::alert_search = {nullptr}; }
        for (size_t i0 = 0; i0 < 3; ++i0)
            { std::destroy_at(&::ttc::entries[i0]); 
            new (&::ttc::entries[i0]) std::remove_reference_t<decltype(::ttc::entries[i0])> {}; }
        ::ttc::number_of_entries = {0};
        { std::destroy_at(&::multi::bt); 
        new (&::multi::bt) std::remove_reference_t<decltype(::multi::bt)> {}; }
        for (size_t i0 = 0; i0 < 3; ++i0)
            for (size_t i1 = 0; i1 < 3; ++i1)
                { if (IS_DRAM(::ttc::entries[i0].::ttc::TTCEntry::dirtag[i1]) || IS_IRAM(::ttc::entries[i0].::ttc::TTCEntry::dirtag[i1])) free(const_cast<char *>(::ttc::entries[i0].::ttc::TTCEntry::dirtag[i1]));
                ::ttc::entries[i0].::ttc::TTCEntry::dirtag[i1] = {nullptr}; }
        { std::destroy_at(&::multi::bt.::multi::BlockThing::block); 
        new (&::multi::bt.::multi::BlockThing::block) std::remove_reference_t<decltype(::multi::bt.::multi::BlockThing::block)> {}; }
    }
    
    // ACTUAL PARSER
    void parse_config(json::TextCallback&& tcb) {
        cfgstate curnode = cfgstate::R;
        
        clear_config();
        json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
            switch (curnode) {
            jpto_R:
            case cfgstate::R:
                curnode = cfgstate::R;
                
                // Delegate('ttc', -2, []){Store('alert_search', -2, []){ttc::alert_search [variable]@[]},Delegate('entries', -1, [<__main__.UpdateSizeRef object at 0x7f0bde473160>]){Store('name', -2, []){void ttc::update_ttc_entry_name(size_t n, char const * value) [free function] {'n': 2}},Store('dirtag', -1, [<__main__.UpdateSizeRef object at 0x7f0bde46d820>]){ttc::entries [variable]@[$2] -> ttc::TTCEntry::dirtag [variable]@[$3]},Store('stopid', -2, []){ttc::entries [variable]@[$2] -> ttc::TTCEntry::stopid [variable]@[]}}}
                if (stack_ptr >= 2 && !strcmp(stack[1]->name, "ttc")) {
                    goto jpto_R__TTC;
                }
                // Delegate('block', -2, []){Store('name', -2, []){multi::bt [variable]@[] -> multi::BlockThing::name [variable]@[]},Store('block', -1, [<__main__.EnsureLazyInit object at 0x7f0bde47f5e0>, <__main__.UpdateSizeRef object at 0x7f0bde47f130>]){multi::bt [variable]@[] -> multi::BlockThing::block [variable]@[$2]}}
                else if (stack_ptr >= 2 && !strcmp(stack[1]->name, "block")) {
                    goto jpto_R__BLOCK;
                }
                else if (stack_ptr == 1) {
                    return;
                }
                else { ESP_LOGW(TAG, "unknown tag in config %s", stack[stack_ptr-1]->name); }
                return;
                
            jpto_R__TTC:
            case cfgstate::R__TTC:
                curnode = cfgstate::R__TTC;
                
                // Store('alert_search', -2, []){ttc::alert_search [variable]@[]}
                if (stack_ptr == 3 && !strcmp(stack[2]->name, "alert_search")) {
                    ::ttc::alert_search = strdup(v.str_val);
                }
                // Delegate('entries', -1, [<__main__.UpdateSizeRef object at 0x7f0bde473160>]){Store('name', -2, []){void ttc::update_ttc_entry_name(size_t n, char const * value) [free function] {'n': 2}},Store('dirtag', -1, [<__main__.UpdateSizeRef object at 0x7f0bde46d820>]){ttc::entries [variable]@[$2] -> ttc::TTCEntry::dirtag [variable]@[$3]},Store('stopid', -2, []){ttc::entries [variable]@[$2] -> ttc::TTCEntry::stopid [variable]@[]}}
                else if (stack_ptr >= 3 && !strcmp(stack[2]->name, "entries") && stack[2]->is_array()) {
                    goto jpto_R__TTC__ENTRIES_ALL;
                }
                else if (stack_ptr == 2) {
                    curnode = cfgstate::R;
                    return;
                }
                else { ESP_LOGW(TAG, "unknown tag in config %s", stack[stack_ptr-1]->name); }
                return;
                
            jpto_R__TTC__ENTRIES_ALL:
            case cfgstate::R__TTC__ENTRIES_ALL:
                ::ttc::number_of_entries = stack[2]->index;
                curnode = cfgstate::R__TTC__ENTRIES_ALL;
                
                // Store('name', -2, []){void ttc::update_ttc_entry_name(size_t n, char const * value) [free function] {'n': 2}}
                if (stack_ptr == 4 && !strcmp(stack[3]->name, "name")) {
                    ::ttc::update_ttc_entry_name(stack[2]->index, v.str_val);
                }
                // Store('dirtag', -1, [<__main__.UpdateSizeRef object at 0x7f0bde46d820>]){ttc::entries [variable]@[$2] -> ttc::TTCEntry::dirtag [variable]@[$3]}
                else if (stack_ptr == 4 && !strcmp(stack[3]->name, "dirtag") && stack[3]->is_array()) {
                    ::ttc::entries[stack[2]->index].::ttc::TTCEntry::amount = stack[3]->index;
                    ::ttc::entries[stack[2]->index].::ttc::TTCEntry::dirtag[stack[3]->index] = strdup(v.str_val);
                }
                // Store('stopid', -2, []){ttc::entries [variable]@[$2] -> ttc::TTCEntry::stopid [variable]@[]}
                else if (stack_ptr == 4 && !strcmp(stack[3]->name, "stopid")) {
                    ::ttc::entries[stack[2]->index].::ttc::TTCEntry::stopid = v.int_val;
                }
                else if (stack_ptr == 3) {
                    curnode = cfgstate::R__TTC;
                    return;
                }
                else { ESP_LOGW(TAG, "unknown tag in config %s", stack[stack_ptr-1]->name); }
                return;
                
            jpto_R__BLOCK:
            case cfgstate::R__BLOCK:
                curnode = cfgstate::R__BLOCK;
                
                // Store('name', -2, []){multi::bt [variable]@[] -> multi::BlockThing::name [variable]@[]}
                if (stack_ptr == 3 && !strcmp(stack[2]->name, "name")) {
                    ::multi::bt.::multi::BlockThing::name = strdup(v.str_val);
                }
                // Store('block', -1, [<__main__.EnsureLazyInit object at 0x7f0bde47f5e0>, <__main__.UpdateSizeRef object at 0x7f0bde47f130>]){multi::bt [variable]@[] -> multi::BlockThing::block [variable]@[$2]}
                else if (stack_ptr == 3 && !strcmp(stack[2]->name, "block") && stack[2]->is_array()) {
                    if (!::multi::bt.::multi::BlockThing::block) ::multi::bt.::multi::BlockThing::block = new ::config::lazy_t<unsigned char [64]> {};
                    ::multi::bt.::multi::BlockThing::block_size = stack[2]->index;
                    (*::multi::bt.::multi::BlockThing::block)[stack[2]->index] = v.int_val;
                }
                else if (stack_ptr == 2) {
                    curnode = cfgstate::R;
                    return;
                }
                else { ESP_LOGW(TAG, "unknown tag in config %s", stack[stack_ptr-1]->name); }
                return;
                
            }
        });
        
        if (!parser.parse(std::move(tcb))) {
            ESP_LOGE(TAG, "Failed to parse JSON for config");
        }
    }
}
