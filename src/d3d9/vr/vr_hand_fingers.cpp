#include <cstring>

#include "vr_hand_fingers.h"

namespace dxvk {

  namespace {

    struct FingerBone {
      const char*    name;
      VrFinger       finger;
      VrGameRotation open;
      VrGameRotation closed;
    };

    /**
     * \brief Local rotations of the left hand's finger bones
     *
     * Open: the free hand's relaxed pose, measured with the 9mm pistol
     * (2026-10-11, findings/weapon.md 6.3). Closed: the game's fists, with
     * no weapon equipped (2026-10-21, findings/fingers.md 3).
     */
    const FingerBone FingerBones[VrFingerBoneCount] = {
      { "Thumb1", VrFinger::Thumb,
        {{ {   0.818252f,   0.384053f,   0.427746f },
           {   0.366491f,   0.224751f,  -0.902869f },
           {  -0.442886f,   0.895539f,  0.0431507f } }},
        {{ {   0.793528f,   0.387627f,   0.469105f },
           {   0.406711f,   0.235591f,  -0.882657f },
           {  -0.452658f,   0.891203f,  0.0292963f } }} },
      { "Thumb11", VrFinger::Thumb,
        {{ {   0.967436f,  -0.253117f,        0.0f },
           {   0.253117f,   0.967436f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }},
        {{ {   0.554083f,  -0.832462f,        0.0f },
           {   0.832462f,   0.554083f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }} },
      { "Thumb12", VrFinger::Thumb,
        {{ {   0.994304f,  -0.106582f,        0.0f },
           {   0.106582f,   0.994304f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }},
        {{ {   0.831972f,  -0.554818f,        0.0f },
           {   0.554818f,   0.831972f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }} },
      { "Hand01", VrFinger::Index,
        {{ {   0.991822f,  0.0157553f,   0.126654f },
           { -0.0121424f,   0.999498f, -0.0292477f },
           {  -0.127052f,  0.0274707f,   0.991516f } }},
        {{ {   0.993915f,  0.0157553f,   0.109013f },
           { -0.0126601f,   0.999498f, -0.0290274f },
           {  -0.109416f,  0.0274707f,   0.993616f } }} },
      { "Finger1", VrFinger::Index,
        {{ {   0.997184f, 0.00861518f,  -0.074502f },
           { -0.0105104f,    0.99963f, -0.0250837f },
           {  0.0742583f,  0.0257961f,   0.996905f } }},
        {{ {   0.603985f,  -0.796683f,  0.0223371f },
           {   0.790319f,    0.60231f,   0.112335f },
           {  -0.102949f,  -0.050195f,   0.993419f } }} },
      { "Finger11", VrFinger::Index,
        {{ {   0.985859f,   -0.16757f, -0.00119342f },
           {   0.167571f,    0.98586f, 0.000681602f },
           { 0.00106233f, -0.000871946f,   0.999999f } }},
        {{ {  -0.403585f,  -0.914941f, -0.00119333f },
           {   0.914941f,  -0.403586f, 0.000681655f },
           { -0.00110529f, -0.000816721f,   0.999999f } }} },
      { "Finger12", VrFinger::Index,
        {{ {   0.999859f,  0.0156992f,  0.0060174f },
           { -0.0157025f,   0.999877f, 0.000501849f },
           { -0.00600878f, -0.000596266f,   0.999982f } }},
        {{ {  0.0997658f,  -0.994993f, 0.00601746f },
           {    0.99501f,  0.0997705f, 0.000501807f },
           { -0.00109966f, 0.00593737f,   0.999982f } }} },
      { "Hand02", VrFinger::Middle,
        {{ {   0.994522f,   0.104404f, 0.00510071f },
           {  -0.104171f,   0.985903f,   0.130934f },
           { 0.00864123f,  -0.130748f,   0.991378f } }},
        {{ {   0.994534f,   0.104405f, -0.00157492f },
           {   -0.10329f,   0.985903f,    0.13163f },
           {  0.0152955f,  -0.130748f,   0.991298f } }} },
      { "Finger2", VrFinger::Middle,
        {{ {   0.988127f,  -0.153207f, -0.0114936f },
           {   0.152233f,   0.986443f, -0.0612823f },
           {  0.0207266f,  0.0588051f,   0.998054f } }},
        {{ {   0.380498f,  -0.923671f,  0.0453195f },
           {   0.920385f,   0.383007f,  0.0787207f },
           { -0.0900697f,  0.0117584f,   0.995866f } }} },
      { "Finger21", VrFinger::Middle,
        {{ {    0.99929f, -0.0376753f, 0.000116667f },
           {  0.0376753f,    0.99929f,        0.0f },
           { -0.000116585f,        0.0f,        1.0f } }},
        {{ {  -0.128803f,   -0.99167f, 0.000116655f },
           {    0.99167f,  -0.128803f,        0.0f },
           {        0.0f, 0.000115694f,        1.0f } }} },
      { "Finger22", VrFinger::Middle,
        {{ {   0.999258f,  0.0385054f,        0.0f },
           { -0.0385054f,   0.999258f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }},
        {{ {   0.220757f,  -0.975329f,        0.0f },
           {   0.975329f,   0.220757f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }} },
      { "Hand03", VrFinger::Ring,
        {{ {   0.991125f,  0.0600862f,   -0.11858f },
           { -0.0447177f,   0.990733f,   0.128255f },
           {   0.125188f,  -0.121814f,   0.984627f } }},
        {{ {   0.992622f,  0.0600863f,  -0.105314f },
           { -0.0464291f,   0.990733f,   0.127645f },
           {   0.112008f,  -0.121814f,   0.986213f } }} },
      { "Finger3", VrFinger::Ring,
        {{ {   0.992671f,  -0.120352f,  0.0109717f },
           {   0.120378f,   0.992727f, -0.00169364f },
           { -0.0106881f, 0.00300198f,   0.999938f } }},
        {{ {   0.340932f,  -0.937414f,  0.0708574f },
           {   0.925141f,   0.347944f,   0.151822f },
           {  -0.166975f,  0.0137921f,   0.985865f } }} },
      { "Finger31", VrFinger::Ring,
        {{ {   0.998236f, -0.0586457f,  0.0092844f },
           {  0.0586366f,   0.998279f, 0.00125062f },
           { -0.00934176f, -0.000704007f,   0.999956f } }},
        {{ { -0.0836475f,  -0.996452f, 0.00928439f },
           {   0.996495f, -0.0836394f, 0.00125057f },
           { -0.000469595f, 0.00935646f,   0.999956f } }} },
      { "Finger32", VrFinger::Ring,
        {{ {   0.991462f,   0.130393f,        0.0f },
           {  -0.130393f,   0.991462f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }},
        {{ {   0.213347f,  -0.976977f,        0.0f },
           {   0.976977f,   0.213347f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }} },
      { "Hand04", VrFinger::Little,
        {{ {   0.973566f,  0.0902254f,  -0.209831f },
           { -0.0499923f,   0.980572f,   0.189685f },
           {   0.222868f,  -0.174181f,   0.959162f } }},
        {{ {   0.980063f,  0.0902255f,  -0.177017f },
           { -0.0563343f,   0.980572f,   0.187899f },
           {   0.190531f,  -0.174181f,   0.966105f } }} },
      { "Finger4", VrFinger::Little,
        {{ {   0.992131f,  -0.120628f,  0.0335495f },
           {    0.12138f,   0.992377f, -0.0213405f },
           { -0.0307195f,  0.0252448f,   0.999209f } }},
        {{ {   0.154909f,  -0.984412f,  0.0832794f },
           {   0.969288f,   0.167744f,    0.17984f },
           {  -0.191007f,  0.0528627f,   0.980164f } }} },
      { "Finger41", VrFinger::Little,
        {{ {   0.990757f,  -0.135646f, -0.000347379f },
           {   0.135646f,   0.990757f, 0.00097353f },
           { 0.000212113f, -0.00101165f,   0.999999f } }},
        {{ { -0.0518084f,  -0.998657f, -0.000347319f },
           {   0.998657f, -0.0518087f, 0.000973536f },
           { -0.000990223f, -0.000296415f,   0.999999f } }} },
      { "Finger42", VrFinger::Little,
        {{ {   0.999922f,  -0.012493f, -6.19373e-05f },
           {   0.012493f,   0.999922f,        0.0f },
           { 6.22444e-05f,        0.0f,        1.0f } }},
        {{ {   0.071679f,  -0.997428f,        0.0f },
           {   0.997428f,   0.071679f,        0.0f },
           {        0.0f,        0.0f,        1.0f } }} },
    };

    constexpr const char* LeftPrefix  = "Bip01 L ";
    constexpr const char* RightPrefix = "Bip01 R ";
    constexpr size_t      PrefixSize  = 8;

  }


  bool vrFindFingerBone(const char* name, VrHand& hand, uint32_t& index) {
    if (!std::strncmp(name, LeftPrefix, PrefixSize))
      hand = VrHand::Left;
    else if (!std::strncmp(name, RightPrefix, PrefixSize))
      hand = VrHand::Right;
    else
      return false;

    for (uint32_t i = 0; i < VrFingerBoneCount; i++) {
      if (!std::strcmp(name + PrefixSize, FingerBones[i].name)) {
        index = i;
        return true;
      }
    }

    return false;
  }


  VrFinger vrFingerOfBone(uint32_t index) {
    return FingerBones[index].finger;
  }


  VrGameRotation vrPoseFingerBone(uint32_t index, VrHand hand, const VrFingerCurls& curls) {
    const FingerBone& bone = FingerBones[index];
    float curl = curls[uint32_t(bone.finger)];

    VrGameRotation rotate;

    if (curl <= 0.0f)
      rotate = bone.open;
    else if (curl >= 1.0f)
      rotate = bone.closed;
    else {
      rotate = vrGameRotationFromQuaternion(vrSlerp(
        vrQuaternionFromGameRotation(bone.open),
        vrQuaternionFromGameRotation(bone.closed), curl));
    }

    return hand == VrHand::Left ? rotate : vrMirrorHandRotation(rotate);
  }

}
