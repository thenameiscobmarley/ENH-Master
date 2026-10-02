#pragma once

#include "PanelArtwork.h"
#include "../../DSP/PatchBay.h"
#include <array>
#include <vector>

/*  The backs of the rack's units, seen when the rack is turned round: where each one's connectors are (the
    patch cables plug into these) and the printed, labelled steel they are mounted in.

    Positions are panel-local, as the faceplate's (x across, z down), on the back plate at y = backPlateY.
    Seen from behind, +x is on the viewer's left. */
namespace pad::backs
{
    enum class Jack { xlrIn, xlrOut, trsIn, trsOut, iec, ground, cord, rocker, fuse };   // cord: a mains cord fixed through a grommet

    /** Who made a unit, and how their backs look (BackPanels.cpp). */
    struct Maker
    {
        const char* name;
        const char* made;                    // "MADE IN ..." (their own way of saying it)
        juce::uint32 steel, ink, dim, accent;
        int face;                            // lettering: 0 sans, 1 serif (and hammertone/crackle paint), 2 mono
        bool powerRight, outputsFirst, trs;  // mains on the right (seen from behind); outputs before inputs; 1/4" links
        int mains;                           // 0 IEC inlet, 1 IEC with its own rocker switch, 2 a cord fixed through a grommet
        int vents;                           // 0 slots, 1 round holes, 2 hex mesh, 3 fan, 4 none, 5 louvres
        int plate;                           // 0 brushed plate, 1 paper sticker, 2 printed on the steel, 3 riveted brass
        int warning;                         // 0 red CAUTION band, 1 yellow sticker, 2 printed text, 3 two languages
        int screws;                          // 0 cross, 1 black hex socket, 2 slotted
        int ground;                          // 0 brass lug, 1 binding post
        int badge;                           // 0 box, 1 oval, 2 wordmark over a rule, 3 diamond
    };
    inline constexpr int numMakers = 10;
    const std::array<Maker, numMakers>& makers();
    int makerOf (int unit);

    struct BackJack
    {
        Jack kind;
        int channel;   // 0 = left, 1 = right (audio); 0 otherwise
        float x, z;    // panel-local centre
    };

    /** Half the back plate's size (it closes the body's open rear) and how far back it sits. */
    float plateHalfW();
    float plateHalfH (int unit);
    float plateY();

    /** Every connector on a unit's back. */
    std::vector<BackJack> jacksOf (int unit);

    /** The back's print and painted hardware: RGBA, gamma-encoded colour; already mirrored so that it reads
        the right way round from behind when mapped like the faceplate (uv = (x, z) over the plate). */
    artwork::RawTexture renderBack (int unit, int textureWidth);

    /*  The patch bay (layout::bayToWorld): a 1U Studio TT (bantam) bay at the bottom of the case, facing the back,
        flush with the units' backs; from the front a blank plate. Two rows of 48: the top row is the units'
        outputs, the bottom row their inputs, a pair of columns (L, R) per unit in the rack's order. */
    namespace bay
    {
        inline constexpr int columns = 48;
        inline constexpr int rackColumn = columns - 2;
        inline constexpr float lampX = 0.0f, lampZ = -0.255f;   // CHAIN OPEN: lit while a cord out of the chain has it silent   // the last pair: RACK IN on the top row, RACK OUT on the bottom
        float halfW();                 // its panel, ears and all
        float faceY();                 // panel-local y of its jack face
        float jackX (int column);      // panel-local x of a column (seen from behind, column 0 is on the left)
        float rowZ (int row);          // 0 = outputs (top), 1 = inputs

        /** Which column pair each unit has: the rack's units in order (POWER has no audio), at most 23. */
        std::vector<int> unitsInOrder();

        /** The jack a cord end is plugged into (false: loose, or nothing on the bay for it), and the port a jack is. */
        bool jackOf (const std::vector<int>& chain, enh::patch::End e, int& col, int& row);
        enh::patch::End portAt (const std::vector<int>& chain, int col, int row);

        artwork::RawTexture renderFace (int textureWidth);    // the jack face's print (mirrored, as the backs')
        juce::Image renderFaceImage (int textureWidth);       // ... as seen from behind (the 2D rack's patch bay)
        /** A cord's end may go into this jack: it's free, and (unless ANYTHING) an OUT's other end goes into an IN. */
        bool canGoInto (const enh::patch::State& s, const std::vector<int>& chain, int cord, int end, int col, int row);
        artwork::RawTexture renderFront (int textureWidth);   // the blank plate in front
    }

    /** The tape flags' writing, cell after cell (cols across): masking tape, a marker's hand. */
    artwork::RawTexture renderFlagAtlas (const std::vector<juce::String>& texts, int cellW, int cellH, int cols);

    /** The MASTER switch's plate (PatchCables.h masterFrame): ANYTHING INTO ANYTHING. */
    artwork::RawTexture renderMasterPlate (int textureWidth, float halfW, float halfH, float leverX);
}
