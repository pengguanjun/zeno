#include <Hg/Archive.hpp>
namespace {
static char data[] = {
117,110,105,102,111,114,109,32,109,97,116,52,32,109,86,80,59,10,117,110,105,102,111,114,109,32,109,97,116,52,32,109,73,110,118,86,80,59,10,117,110,105,102,111,114,109,32,109,97,116,52,32,109,86,105,101,119,59,10,117,110,105,102,111,114,109,32,109,97,116,52,32,109,80,114,111,106,59,10,10,105,110,32,118,101,99,51,32,112,111,115,105,116,105,111,110,59,10,105,110,32,118,101,99,50,32,116,101,120,99,111,111,114,100,59,10,105,110,32,118,101,99,51,32,105,78,111,114,109,97,108,59,10,10,111,117,116,32,118,101,99,52,32,102,67,111,108,111,114,59,10,10,115,116,114,117,99,116,32,76,105,103,104,116,32,123,10,32,32,118,101,99,51,32,100,105,114,59,10,32,32,118,101,99,51,32,99,111,108,111,114,59,10,125,59,10,10,115,116,114,117,99,116,32,77,97,116,101,114,105,97,108,32,123,10,32,32,118,101,99,51,32,97,108,98,101,100,111,59,10,32,32,102,108,111,97,116,32,114,111,117,103,104,110,101,115,115,59,10,32,32,102,108,111,97,116,32,109,101,116,97,108,108,105,99,59,10,32,32,102,108,111,97,116,32,115,112,101,99,117,108,97,114,59,10,125,59,10,10,118,101,99,51,32,112,98,114,40,77,97,116,101,114,105,97,108,32,109,97,116,101,114,105,97,108,44,32,118,101,99,51,32,110,114,109,44,32,118,101,99,51,32,105,100,105,114,44,32,118,101,99,51,32,111,100,105,114,41,32,123,10,32,32,102,108,111,97,116,32,114,111,117,103,104,110,101,115,115,32,61,32,109,97,116,101,114,105,97,108,46,114,111,117,103,104,110,101,115,115,59,10,32,32,102,108,111,97,116,32,109,101,116,97,108,108,105,99,32,61,32,109,97,116,101,114,105,97,108,46,109,101,116,97,108,108,105,99,59,10,32,32,102,108,111,97,116,32,115,112,101,99,117,108,97,114,32,61,32,109,97,116,101,114,105,97,108,46,115,112,101,99,117,108,97,114,59,10,32,32,118,101,99,51,32,97,108,98,101,100,111,32,61,32,109,97,116,101,114,105,97,108,46,97,108,98,101,100,111,59,10,10,32,32,118,101,99,51,32,104,100,105,114,32,61,32,110,111,114,109,97,108,105,122,101,40,105,100,105,114,32,43,32,111,100,105,114,41,59,10,32,32,102,108,111,97,116,32,78,111,72,32,61,32,109,97,120,40,48,44,32,100,111,116,40,104,100,105,114,44,32,110,114,109,41,41,59,10,32,32,102,108,111,97,116,32,78,111,76,32,61,32,109,97,120,40,48,44,32,100,111,116,40,105,100,105,114,44,32,110,114,109,41,41,59,10,32,32,102,108,111,97,116,32,78,111,86,32,61,32,109,97,120,40,48,44,32,100,111,116,40,111,100,105,114,44,32,110,114,109,41,41,59,10,32,32,102,108,111,97,116,32,86,111,72,32,61,32,99,108,97,109,112,40,100,111,116,40,111,100,105,114,44,32,104,100,105,114,41,44,32,48,44,32,49,41,59,10,32,32,102,108,111,97,116,32,76,111,72,32,61,32,99,108,97,109,112,40,100,111,116,40,105,100,105,114,44,32,104,100,105,114,41,44,32,48,44,32,49,41,59,10,10,32,32,118,101,99,51,32,102,48,32,61,32,109,101,116,97,108,108,105,99,32,42,32,97,108,98,101,100,111,32,43,32,40,49,32,45,32,109,101,116,97,108,108,105,99,41,32,42,32,48,46,49,54,32,42,32,115,112,101,99,117,108,97,114,32,42,32,115,112,101,99,117,108,97,114,59,10,32,32,118,101,99,51,32,102,100,102,32,61,32,102,48,32,43,32,40,49,32,45,32,102,48,41,32,42,32,112,111,119,40,49,32,45,32,86,111,72,44,32,53,41,59,10,10,32,32,102,108,111,97,116,32,107,32,61,32,40,114,111,117,103,104,110,101,115,115,32,43,32,49,41,32,42,32,40,114,111,117,103,104,110,101,115,115,32,43,32,49,41,32,47,32,56,59,10,32,32,102,108,111,97,116,32,118,100,102,32,61,32,48,46,50,53,32,47,32,40,40,78,111,86,32,42,32,107,32,43,32,49,32,45,32,107,41,32,42,32,40,78,111,76,32,42,32,107,32,43,32,49,32,45,32,107,41,41,59,10,10,32,32,102,108,111,97,116,32,97,108,112,104,97,50,32,61,32,109,97,120,40,48,44,32,114,111,117,103,104,110,101,115,115,32,42,32,114,111,117,103,104,110,101,115,115,41,59,10,32,32,102,108,111,97,116,32,100,101,110,111,109,32,61,32,49,32,45,32,78,111,72,32,42,32,78,111,72,32,42,32,40,49,32,45,32,97,108,112,104,97,50,41,59,10,32,32,102,108,111,97,116,32,110,100,102,32,61,32,97,108,112,104,97,50,32,47,32,40,100,101,110,111,109,32,42,32,100,101,110,111,109,41,59,10,10,32,32,118,101,99,51,32,98,114,100,102,32,61,32,102,100,102,32,42,32,118,100,102,32,42,32,110,100,102,32,42,32,102,48,32,43,32,40,49,32,45,32,102,48,41,32,42,32,97,108,98,101,100,111,59,10,32,32,114,101,116,117,114,110,32,98,114,100,102,32,42,32,78,111,76,59,10,125,10,10,118,101,99,51,32,99,97,108,99,82,97,121,68,105,114,40,118,101,99,51,32,112,111,115,41,10,123,10,32,32,118,101,99,52,32,118,112,111,115,32,61,32,109,86,80,32,42,32,118,101,99,52,40,112,111,115,44,32,49,41,59,10,32,32,118,101,99,50,32,117,118,32,61,32,118,112,111,115,46,120,121,32,47,32,118,112,111,115,46,119,59,10,32,32,118,101,99,52,32,114,111,32,61,32,109,73,110,118,86,80,32,42,32,118,101,99,52,40,117,118,44,32,45,49,44,32,49,41,59,10,32,32,118,101,99,52,32,114,101,32,61,32,109,73,110,118,86,80,32,42,32,118,101,99,52,40,117,118,44,32,43,49,44,32,49,41,59,10,32,32,118,101,99,51,32,114,100,32,61,32,110,111,114,109,97,108,105,122,101,40,114,101,46,120,121,122,32,47,32,114,101,46,119,32,45,32,114,111,46,120,121,122,32,47,32,114,111,46,119,41,59,10,32,32,114,101,116,117,114,110,32,114,100,59,10,125,10,10,118,111,105,100,32,109,97,105,110,40,41,10,123,10,32,32,118,101,99,51,32,110,111,114,109,97,108,32,61,32,110,111,114,109,97,108,105,122,101,40,105,78,111,114,109,97,108,41,59,10,32,32,118,101,99,51,32,118,105,101,119,100,105,114,32,61,32,45,99,97,108,99,82,97,121,68,105,114,40,112,111,115,105,116,105,111,110,41,59,10,10,32,32,77,97,116,101,114,105,97,108,32,109,97,116,101,114,105,97,108,59,10,32,32,109,97,116,101,114,105,97,108,46,97,108,98,101,100,111,32,61,32,68,95,65,76,66,69,68,79,59,10,32,32,109,97,116,101,114,105,97,108,46,114,111,117,103,104,110,101,115,115,32,61,32,68,95,82,79,85,71,72,78,69,83,83,59,10,32,32,109,97,116,101,114,105,97,108,46,109,101,116,97,108,108,105,99,32,61,32,68,95,77,69,84,65,76,76,73,67,59,10,32,32,109,97,116,101,114,105,97,108,46,115,112,101,99,117,108,97,114,32,61,32,48,46,53,59,10,10,32,32,76,105,103,104,116,32,108,105,103,104,116,59,10,32,32,108,105,103,104,116,46,100,105,114,32,61,32,110,111,114,109,97,108,105,122,101,40,40,109,86,80,32,42,32,118,101,99,52,40,45,49,44,32,45,50,44,32,53,44,32,48,41,41,46,120,121,122,41,59,10,32,32,108,105,103,104,116,46,100,105,114,32,61,32,102,97,99,101,102,111,114,119,97,114,100,40,108,105,103,104,116,46,100,105,114,44,32,45,108,105,103,104,116,46,100,105,114,44,32,110,111,114,109,97,108,41,59,10,32,32,108,105,103,104,116,46,99,111,108,111,114,32,61,32,118,101,99,51,40,49,44,32,49,44,32,49,41,59,10,10,32,32,118,101,99,51,32,115,116,114,101,110,103,116,104,32,61,32,112,98,114,40,109,97,116,101,114,105,97,108,44,32,110,111,114,109,97,108,44,32,108,105,103,104,116,46,100,105,114,44,32,118,105,101,119,100,105,114,41,59,10,32,32,118,101,99,51,32,99,111,108,111,114,32,61,32,108,105,103,104,116,46,99,111,108,111,114,32,42,32,115,116,114,101,110,103,116,104,59,10,32,32,102,67,111,108,111,114,32,61,32,118,101,99,52,40,99,111,108,111,114,44,32,49,46,48,41,59,10,125,10
};
static int res = hg::Archive::add("mesh.frag", data, sizeof(data));
}
