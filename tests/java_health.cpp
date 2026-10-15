#include "network/java/JavaHealth.h"
#include <cassert>
#include <limits>
int main(){assert(javaNativeHealth(20)==20);assert(javaNativeHealth(19.1)==20);assert(javaNativeHealth(1.75)==2);assert(javaNativeHealth(.75)==1);assert(javaNativeHealth(.06)==1);assert(javaNativeHealth(0)==0);assert(javaNativeHealth(-1)==0);assert(javaNativeHealth(std::numeric_limits<double>::quiet_NaN())==0);assert(javaNativeHealth(1e100)==std::numeric_limits<int>::max());}
