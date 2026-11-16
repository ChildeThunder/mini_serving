#include "HttpProtocol.h"
#include "HttpServer.h"
#include "InferenceHandler.h"

using namespace std;
int main()
{
    HttpServer hs(6666);
    hs.run();
    return 0;
}