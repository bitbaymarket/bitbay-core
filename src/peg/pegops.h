// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITBAY_PEGOPS_H
#define BITBAY_PEGOPS_H

//#ifdef __cplusplus
//extern "C"
//{
//#endif // __cplusplus

void getpeglevel();
void updatepegbalances();
void movecoins();
void moveliquid();
void movereserve();

//#ifdef __cplusplus
//}
//#endif // __cplusplus
#endif // BITBAY_PEGOPS_H
