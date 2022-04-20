#ifndef ENTITYCORE_GLOBALS_HPP_
#define ENTITYCORE_GLOBALS_HPP_

enum class Implicit : uint8_t
{
    NOTHING = 0x00,
    SRC_LAYOUT = 0x01,
    DST_LAYOUT = 0x02,
    LAYOUT = SRC_LAYOUT | DST_LAYOUT,
};

#define ImplicitFilter(value, filter) ((Implicit) ((uint8_t) value & (uint8_t) Implicit::filter))

#endif /* end of include guard: ENTITYCORE_GLOBALS_HPP_ */
