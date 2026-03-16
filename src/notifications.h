#pragma once

#include <string>
#include <functional>

void notifications_init();
void notifications_set_click_handler(std::function<void()> handler);
void notifications_show(const std::string& title, const std::string& body);
void notifications_shutdown();
