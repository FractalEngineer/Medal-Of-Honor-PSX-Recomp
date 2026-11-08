#include "moh_mkb.h" /* Host action and wheel handling; not a plugin test. */
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void) {
    MohMkbControls controls;
    uint8_t keys[512] = {0};
    moh_mkb_controls_reset(&controls);
    CHECK(!strcmp(moh_mkb_action_id(MOH_MKB_FIRE), "fire"));
    CHECK(moh_mkb_action_id((MohMkbAction)-1) == NULL);
    CHECK(controls.bindings[MOH_MKB_MENU_CONFIRM].count == 3);
    keys[40] = 1;
    CHECK(moh_mkb_action_down(&controls, MOH_MKB_FIRE, keys, sizeof(keys), 0));
    CHECK(!moh_mkb_action_down(&controls, MOH_MKB_FIRE, keys, 40, 0));
    keys[40] = 0;
    CHECK(moh_mkb_action_down(&controls, MOH_MKB_FIRE, keys, sizeof(keys), 1));
    CHECK(moh_mkb_controls_set(&controls, MOH_MKB_FIRE, "key:4,mouse:3,key:7"));
    keys[7] = 1;
    CHECK(moh_mkb_action_down(&controls, MOH_MKB_FIRE, keys, sizeof(keys), 0));
    CHECK(!moh_mkb_controls_set(&controls, MOH_MKB_FIRE, "wheel:up"));
    CHECK(!moh_mkb_controls_set(&controls, MOH_MKB_FIRE, "key:1,key:2,key:3,key:4"));
    CHECK(moh_mkb_action_down(&controls, MOH_MKB_FIRE, keys, sizeof(keys), 0));
    CHECK(moh_mkb_controls_set(&controls, MOH_MKB_FIRE, "none"));
    CHECK(!moh_mkb_action_down(&controls, MOH_MKB_FIRE, keys, sizeof(keys), 31));

    moh_mkb_controls_wheel(&controls, -0.4);
    CHECK(moh_mkb_controls_take_wheel(&controls) == 0);
    moh_mkb_controls_wheel(&controls, -0.6);
    CHECK(moh_mkb_controls_take_wheel(&controls) == 1);
    moh_mkb_controls_wheel(&controls, 2.0);
    CHECK(moh_mkb_controls_take_wheel(&controls) == -1);
    CHECK(moh_mkb_controls_take_wheel(&controls) == -1);
    CHECK(moh_mkb_controls_take_wheel(&controls) == 0);
    CHECK(moh_mkb_controls_set(&controls, MOH_MKB_NEXT_WEAPON, "wheel:up"));
    CHECK(moh_mkb_controls_set(&controls, MOH_MKB_PREV_WEAPON, "wheel:down"));
    moh_mkb_controls_wheel(&controls, 1.0);
    CHECK(moh_mkb_controls_take_wheel(&controls) == 1);
    moh_mkb_controls_wheel(&controls, -1000.0);
    for (int i = 0; i < 16; ++i) CHECK(moh_mkb_controls_take_wheel(&controls) == -1);
    CHECK(moh_mkb_controls_take_wheel(&controls) == 0);
    return 0;
}
