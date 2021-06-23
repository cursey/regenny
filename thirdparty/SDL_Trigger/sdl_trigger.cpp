#include "sdl_trigger.h"
#include <iostream>
#include <algorithm>

namespace Trigger {

    std::vector<Group*> groups;
    Group globalGroup;

    bool KeyCombination::hasKey(SDL_Keycode key) const {
        for(size_t i = 0; i < keys.size(); i++) {
            if (keys[i].key == key) {
                return true;
            }
        }

        return false;
    }

    void KeyCombination::markKeyDown(SDL_Keycode key) {
        for(size_t i = 0; i < keys.size(); i++) {
            if (keys[i].key == key) {
                keys[i].isDown = true;
            }
        }
    }

    void KeyCombination::markKeyUp(SDL_Keycode key) {
        for(size_t i = 0; i < keys.size(); i++) {
            if (keys[i].key == key) {
                keys[i].isDown = false;
            }
        }
    }

    void KeyCombination::reset() {
        for(size_t i = 0; i < keys.size(); i++) {
            keys[i].isDown = false;
        }
    }

    bool KeyCombination::isFulfilled() const {
        for(size_t i = 0; i < keys.size(); i++) {
            if (!keys[i].isDown) {
                return false;
            }
        }

        return true;
    }

    Trigger::Trigger(Keycodes keys, Callback callback) : callback{callback} {
        for(const auto key : keys) {
            combination.keys.push_back({key, false});
        }
    }

    Group::Group() : triggers{}, isEnabled{true} {
        groups.push_back(this);
    }

    Group::~Group() {
        groups.erase(std::remove(groups.begin(), groups.end(), this));
    }

    void Group::enable() {
        isEnabled = true;
    }

    void Group::disable() {
        isEnabled = false;

        for (auto& trigger : triggers) {
            trigger.combination.reset();
        }
    }

    void Group::toggle() {
        if (isEnabled) {
            disable();
        } else {
            enable();
        }
    }

    void Group::on(SDL_Keycode key, Callback callback) {
        on(Keycodes{key}, callback);
    }

    void Group::on(Keycodes keys, Callback callback) {
        triggers.push_back(Trigger(keys, callback));
    }

    void Group::processEvent(SDL_Event& e) {
        SDL_Keycode key = e.key.keysym.sym;

        if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            for (auto& trigger : triggers) {
                if (trigger.combination.hasKey(key)) {
                    trigger.combination.markKeyDown(key);
                } else {
                    trigger.combination.reset();
                }
            }

            for (auto& trigger : triggers) {
                if (trigger.combination.isFulfilled()) {
                    trigger.callback();
                }
            }
        } else if (e.type == SDL_KEYUP) {
            for (auto& trigger : triggers) {
                if (trigger.combination.hasKey(key)) {
                    trigger.combination.markKeyUp(key);
                }
            }
        }
    }

    void on(SDL_Keycode key, Callback callback) {
        on(Keycodes{key}, callback);
    }

    void on(Keycodes keys, Callback callback) {
        globalGroup.triggers.push_back(Trigger(keys, callback));
    }

    void processEvent(SDL_Event& e) {
        for (auto& group : groups) {
            if (group->isEnabled) {
                group->processEvent(e);
            }
        }
    }

} // namespace Trigger
