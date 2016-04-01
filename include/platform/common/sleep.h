/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#ifndef SLEEP_H_
#define SLEEP_H_

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

void sleep_start();

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread

#endif  // SLEEP_H_
