#include "menu_callback.h"

#include <cassert>

int main()
{
    Menu menu;
    menu.hItems.push_back({ITEM_DEFAULT, std::string(64, '1'), std::string(64, '%')});
    bool called = false;
    menu.hFunc = [&menu, &called, expected = std::string(64, '%')](const char* value, const char* label, int item, int slot) {
        menu.clear();
        assert(expected == label);
        assert(value == std::string(64, '1'));
        assert(item == 1 && slot == 7);
        called = true;
    };
    InvokeMenu(menu.hFunc, menu.hItems[0].sBack, menu.hItems[0].sText, 1, 7);
    assert(called && !menu.hFunc && menu.hItems.empty());
}
