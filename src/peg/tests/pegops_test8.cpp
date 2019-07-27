// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QtTest/QtTest>

#include "pegops.h"
#include "pegdata.h"
#include "pegops_tests.h"

#include <string>

using namespace std;
using namespace pegops;

namespace pegops {
string packpegdata(const CFractions & fractions,
                   const CPegLevel & peglevel);
bool unpackbalance(CFractions & fractions,
                   CPegLevel & peglevel,
                   const string & pegdata64,
                   string tag,
                   string & err);
bool unpackbalance(const string &   pegdata64,
                   string           tag,
                   CFractions &     fractions,
                   CPegLevel &      peglevel,
                   int64_t &        nReserve,
                   int64_t &        nLiquid,
                   string &         err);
string packpegdata(const CFractions &     fractions,
                   const CPegLevel &      peglevel,
                   int64_t                nReserve,
                   int64_t                nLiquid);
}

void TestPegOps::test8()
{
    string pegdata = "AgACAAAAAAAAAAAA4QsAAAAAAAB42m1aZ1hVVxZ9mGdsBMeCGiyDvWAU4xh1osaooLFjCZbBMqKgSewSZqJGjdEYu8ZK7GJBjKOIdRxsWGIXY4vG3mIBCzaQN++eu1a+99YX/mz2Pueee86ua5/7HI4//7s2acyZ/r4OR2DHkYtLPXO5gue8cVl/yTmGuJ7bxNUz99l81vy/4Ln59yoaegXjfqD/BU0AjY57nVTSPa/z4xOGHxuVuG+um/6M8TGxi4pY68R0Dzf81p+Cwva4+cg+jXLyu+mTTY62m93yF5jfG/vK08De2daLz82+Nre+WdFa0ycswznHzRd82MTsL6Bl/4Awt7w89v1l4SzznCPF5k9j3dYTQg3dhY0/hPwBaLfFyRez3ZTze2C9/qG5HZ56qYvxLfvCDf8x5D0gj1hs053g22P887b7W1rrRw8ec8eiH24ftektt7zSjErLMt18UczLwHNtOn164ZWb5uM+AvIYuvQtm5/bupHR1OapkectWjC/vfN6R28aA6dhndx4nue8A3oY9LXL+69MI+zjVtrqmpY+WzWtbs1ZkjKyjCWPqnbbrO/8NNLMW4TnjoLmwvuOOW06FfJBPlXNznM/fGb4nIYfDbPobxj3x3MDn2Zb6nGdgPxlkq2ZXEujM33cdAvkNTH/CPgKoFC/6+PO5dMtmuRs8NjysxKYv4Rx4Tf2pZmYHmr459X8DK3cvvVQ679UzKsZa2voPvg0+GejYyFmIBfed//KdB8TL7ttOyU06Wsc5MyBlUYlQ+Y7T1rzymGd/CUHxls2WUU/rDndnDvxyY7Z99303IHtRt7COdW88RWeS/0sI8CiRcvYjnB8wmRjj/0PNte3+JFYr9bBdYYuw3M/nHht1ik055KhvUouMeOhmN/qUX2H5Ss9C9geWB3PlQHtNarFJkuP9z+f/cg66DE8l3HJ14xfpP9g/oukwGDrRe9B3vDg3bKWvOG2iBIW/wPkjbj+9eLxxn6R9428frlYs5GA5JkpFn3nsS1PfFrDnDc+aO8uz3y0bppNy1Ursd+ij1dXsMLHFYn1r78aZcaP5WzyN3qFfNsU2yLDs2yHrbXsH2ZewLiH5kgFMI9++i54nvd30HTQDaAIU0c70IVXY80LOB/h4ehN/wW/G/wloTcGLzX+UwTz1la16dA4HzveMI95tzDm7Qc/DvzGbkWzrSd2p+UuaeyEceafzclRWyx6G/InNeyIfgN+LOOrjn3C9eBvMv8FVR1j7IX11kH+AfhfwZd71PyGtUJUm0wTeBGQF+8RaOLyhv8Zs8SMusHGxNT3zeEZg6wdTRsR+szO08OfGL+z1eBYI3aq8t3CZjke+a0y5BNH5D5r4mCjPRqL8Z1IlB3pN5AP753ezZrZAfIx1IdzraHXwO8Djek54q41vwHmM49fCbto/LkP+NG0V3Qlw3eB/JT4F+3aGuPR2YhbBBzPF48EfCe6WCuL0l8W9EXdDjlsVJAQEmVW2LN6pikR3+L5+pi/Ag5xX+rGxsG2/BbryLT+RvNdfowp5fm+1RgvUW65sVMI5H0jUZfmRpmdp6/qalS3IQnr085r7BN3xnOMJ9oJ7u84v96uPJchr00/nmx7IPEN13l/3i8LPOvj3yEnHgjLk1DEM04D+N4nK8w5Ow060sdTfm/GaONADTNODbP4tyHf0fRQXouWT24z2BpPxXqfdV1s1r/XY0I1i6ZMzGvrG+OHdpc2wOtFty6FLLoJ8hPZq02JHIX1T0I+2tnM31Pvja+dsdK0g/FYFvKCjeLNPMbCXtBHjSuYo84FHxcYa0L9cmn7ua8gf4/+C/75BjuDXUfc0T+Ik4jHGBf/fmTTNMlPtOv+3jat5vCmP2F+KfDEVcHg/UDPQD7jiNPs/3xQP/NG5psIzDsEvhh44p+ZoKwLxXfsMSe7ATn11h3jy8AHCb4i3toBvu8am0+kvTA+i3UXfNKz1Fee/ss6wvpTeXT1TM99P2P9lfjkPqmPeuIvSBsu+intwfi5muXM5Wnnp6DMS/THVsdtWgdy4pqvBVd/IH4QLPMrzj1tAHu9dBuh/YJ52ztMzPI8RybrVHvb4RqIf/yV/jFhpqHU2yvJo584/tyv6a8nUEDC4o8Z17q6J8GYNIk4ln0G+JR23nWW9dRv21LzymZcl/mltn2SOeALYbyO+OdV8dsY4ijwrMf3pK51KfHMuEB+mcf15khfQvzU0uFdP+nXXPfRQNuzJmP8MfOHnIPzfUFvxe40BmP8ElcuZL0Hz/3ESd4NFP8NFb9lHxol8bxR/J91ucasQKOSu1Lv9jD/gk6AfAj4kuK3B1jfBA8iHTraguYFZX/UDPQg6F3JA7VALxD3gfpKXaV96fc1JZ8+kXxUCXSvxOEuwaucV0T0wnzdX/bHOOc5zwtOoT/9Dfx4sRf3nyV5rDTrltj1qcxjHqMfrZU+9CtQ4vPvZH/UB/1yRbbt58TTtMckvGg75NSTU3AtcXMVyDMl/xCHkCdOTpX6s1H08KusW1Dueeh/e+Q8ediPQV5E9kV98h6j30rbgyOlDi2bV92Y+LL4OeOntuST23KeAoLfaojdUiDvBJ59fjvpi5hfGI/sn/OI/56U+A6X9zO/MQ5nsC+QuvC79Eesi6wrYZC/K/7PfnH2t1/4eOLzj8RPmU+or6MdzpT1tCvrLv19G2gS5FUlL3J+sNST2cSRUi9Zd5jv2N+dkrpP+7aQvEf8MVD6nHCp/8NAX7JPAaXfZ2C8k8v7fq4x5BUEF/K9FWVfPDfrbj/WHYnbqYIPlkt+iGEfwXso9neQA/78kScSxH/TJM+lCT6YJv7AOl5e7hv43v8JDub+moM2wfgKhzfeXAJKvdFu70ideCn5hnbjPQ37/KFiJ8bJVvAOufdYkOWdX+5IXWb+7gbahzgFPPU6TvRHHDyA+Bnys3Kf0krsWF7wAOs08/oVwd3ENSsh/1rqlZ/kX/rt96Df8P4X/GmJe/bRW8ATV82S9aNBR7yJ88JXvJ9Yl2NrJErq+UTxV+Zd+t/P4EcJniwleIJ1/qTEP/HfANDzkM8D/6HUsxjJq/Pk3nC39En/lDzCe5AUh/f98i6592E/i/bLwXth+sFl1hHBocxjR075ePUtvnLeOPHHVlKPqbd/CX6i/uOlbsWBAn78gTtYZ9dLPEwVfdcWvd2Te5LlYv8Iuf+fIvWQeeuu2JFxkyz9FO/5mov9W0s+Pij16orUgWTBJffkHsNf+oJaUu95b8d6GyH5jTh0vcQzx2mv35iXQuwOKk7uiZo5vc91W/IS9d0LPM8znXkShg0W/2OdGA/gPkXyJfMSceCP7MfS7c4+RNYZIt+zFoIWLpHjlY+YZ6bTXqwboMcFJ44AZT95TfLO6DL2+vwekST49brkWeaF/mJ3zh8pfhXp9M7rzLOnBX8N4fPghwkOTGR/gbhbJf1pXfDnXN7f9Yiju0tfW13qSpDEDfFMYfHDBbLvGLnf8hfcSL2xD2TcbBd/3iT3Bi8FBzYT3HZB5g+T7xDsB45j/BOpe4z3otJ3pAruVntw3TbyHOvsDqk73/D7s8R/Y8nDLsGzVeQ+hXl9ivQhjLu1Eve8n1oq/tFP+h7iJPZRpeS9eSVOiZeayvfkRcQx4L8AP1nye0WpF1sFn7YXu7Dejxf98b6BeYR5J5ec45x87/gPaLh8H0mX/B3r8P6uwOdGS39cTOpttnxn6ii4rqvE9Wu5j74u+PQryZ/E4axzPXa+8erPGa/P5XtMqvRl7Jd4XtpnkNxXZEjdPSl5NlH21VDun1j3Gsh90EK51zgh93X0g/kSD7TzAMHJ+QSv3ZH7ZdpxhKwbLn4YJOsflvzCPmw18a7k9SPyu4UjkmfXSz8eLHncX+6baJ9Q+T3BYYm72xJH16UO9BF82U5wWj3ps0sJbn1b7k3ZjwVKnOUS/obkz8uCg09Jfuc9+VbJG9vkfrO55BPqZajk+VoSx+Wlz3gs++b35xDx81TJQ12kD+C896WO7ha81Vj6mgfy+4JJ7DfO2xmS98vfR9hAy/dLe6c9+b23ivd9EOsV+7VwNAr5fJuYkPw/RZRX/i0zAAAAAAAALDMAAAAAAAADAAMAAwAAAIljRdYBAAAA9omA1GkMAABq7cWqawwAAAGeOYs2MgAA";

    string out_err;
    CPegLevel level1("");
    CFractions user1(0,CFractions::STD);
    int64_t nReserve = 0;
    int64_t nLiquid = 0;
    bool ok = unpackbalance(pegdata, "test", user1, level1, nReserve, nLiquid, out_err);
    
    QVERIFY(ok == true);
    QVERIFY(user1.Total() == (user1.High(level1) + user1.Low(level1)));
    
    CFractions l = user1.LowPart(level1, nullptr);
    CFractions h = user1.HighPart(level1, nullptr);
    
    QVERIFY(l.Total() == user1.Low(level1));
    QVERIFY(h.Total() == user1.High(level1));
    
    //nReserve = 0;
    //nLiquid = user1.Total();
    //qDebug() << QString::fromStdString(packpegdata(user1, level1, nReserve, nLiquid));
}
