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
 *    @author  WenZheng Li  <wenzheng@nestlabs.com>
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#ifndef PLATFORM_EM35X_SLEEP_H_
#define PLATFORM_EM35X_SLEEP_H_

namespace Thread {

class Sleep {
 public:
  static void Begin();
  static void End();
};

}  // namespace Thread

#endif  // PLATFORM_EM35X_SLEEP_H_
