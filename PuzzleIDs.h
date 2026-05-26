// Auto-generated file mapping .bpz filenames to official .sol puzzle IDs
#ifndef PUZZLE_IDS_H
#define PUZZLE_IDS_H

#include <unordered_map>
#include <string>
#include <cstdint>
#include <algorithm>

inline uint32_t getOfficialPuzzleID(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

    static const std::unordered_map<std::string, uint32_t> puzzleMap = {
        {"1. pairs.bpz", 0x845EC60FU},
        {"10. power transfer.bpz", 0x38A3463EU},
        {"11. long reach.bpz", 0x657B7EC9U},
        {"12. cube row 1.bpz", 0x3C2662CU},
        {"13. cancellation.bpz", 0xAF95D3F4U},
        {"14. five down.bpz", 0x542ADDCCU},
        {"15. rocket.bpz", 0xA13A2139U},
        {"16. demolition.bpz", 0xEFFE555FU},
        {"17. buttress.bpz", 0x745D2FB8U},
        {"18. pillar.bpz", 0xE737E851U},
        {"19. red herring.bpz", 0xB57E714CU},
        {"2. above and below.bpz", 0x0U},
        {"20. gathering stones.bpz", 0x49AD9F76U},
        {"21. 5x5 checkerboard minus one.bpz", 0xA9776F36U},
        {"22. 5x5 212 checker.bpz", 0x16F0AE0U},
        {"23. 4x5 211 checker.bpz", 0xD12C8EU},
        {"24. 4x5 checkerboard.bpz", 0x4DA3BE9AU},
        {"25. 5x5 checkerboard.bpz", 0x732C67CEU},
        {"26. green right.bpz", 0xD694A537U},
        {"27. toward middle.bpz", 0xE2A83FE3U},
        {"28. hi and low.bpz", 0xDB316FC4U},
        {"29. broken symmetry.bpz", 0x63B72259U},
        {"3. white meet.bpz", 0xE244B083U},
        {"30. towers.bpz", 0xDAF3BBD8U},
        {"31. countdown.bpz", 0x8E450C7EU},
        {"32. race.bpz", 0xC65A36B9U},
        {"33. test5.bpz", 0x5DE2202FU},
        {"34. race2.bpz", 0x7F5A3816U},
        {"35. delay.bpz", 0xFF09E983U},
        {"36. easy climb.bpz", 0x1B329150U},
        {"37. mound.bpz", 0x89420233U},
        {"38. hill.bpz", 0xF747209U},
        {"39. mountain.bpz", 0x52E9D96FU},
        {"4. house.bpz", 0x8470D1CU},
        {"40. peak.bpz", 0x92FF2700U},
        {"41. b.bpz", 0x7A3D4A37U},
        {"42. j.bpz", 0x354B5DEU},
        {"43. w.bpz", 0xD1E10D94U},
        {"44. l.bpz", 0x22491A15U},
        {"45. d.bpz", 0x4021B7B9U},
        {"46. cool moves.bpz", 0x88E5A8BCU},
        {"47. short fuse.bpz", 0xD0FCAB9BU},
        {"48. long fuse.bpz", 0xE820670DU},
        {"49. leftovers.bpz", 0xDEF0302CU},
        {"5. test1.bpz", 0x19050000U},
        {"50. patience.bpz", 0x5CC01EA2U},
        {"51. rock checker house.bpz", 0xB7323F62U},
        {"52. rock checker tall house.bpz", 0xA5C9331DU},
        {"53. 5x7 rock checker.bpz", 0xF98C17EFU},
        {"54. 6x6 rock checker.bpz", 0xDDBD0EEAU},
        {"55. rock checker triangle.bpz", 0xF7D62C7CU},
        {"56. gaps.bpz", 0x87ABBE3U},
        {"57. valley.bpz", 0xBB62327AU},
        {"58. columns.bpz", 0x6A52CC46U},
        {"59. columns2.bpz", 0x6C395984U},
        {"6. power on.bpz", 0xBAB84665U},
        {"60. columns3.bpz", 0x6DD78980U},
        {"61. 6x6 checker rock diag.bpz", 0x7DAF5F7AU},
        {"62. 6x6 checker split.bpz", 0x93173B06U},
        {"63. 6x6 flag.bpz", 0x2C159B79U},
        {"64. 6x6 checker split2.bpz", 0x5A2AFF47U},
        {"65. 6x6 octagon target.bpz", 0x81BFA91EU},
        {"66. ox.bpz", 0xE66A9007U},
        {"67. test3.bpz", 0xFC91D873U},
        {"68. get the red out.bpz", 0x5B9112DAU},
        {"69. test7.bpz", 0xCC87EF38U},
        {"7. chain reaction.bpz", 0xB9F9DE55U},
        {"70. four rocks.bpz", 0xAB13B237U},
        {"71. 2x2 cell 2x3 checker.bpz", 0x47BB6641U},
        {"72. 2x2 cell 3x3 checker.bpz", 0x683868A5U},
        {"73. 4x8 checkerboard.bpz", 0x662FC197U},
        {"74. 7x4 checkerboard.bpz", 0xE5C358D7U},
        {"76. pyramid power.bpz", 0x8158F851U},
        {"77. easy triangle.bpz", 0xFB7114E1U},
        {"78. small triangle.bpz", 0xBEFA9093U},
        {"79. pairs2.bpz", 0x4A4F2F11U},
        {"8. test2.bpz", 0xE4018A59U},
        {"80. pairs3.bpz", 0xA79EE88AU},
        {"9. power switch.bpz", 0x79302E42U},
    };

    auto it = puzzleMap.find(lower);
    if (it != puzzleMap.end()) {
        return it->second;
    }
    return 0x00000000U; // Default for custom levels
}

#endif // PUZZLE_IDS_H
