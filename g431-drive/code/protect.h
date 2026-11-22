#ifndef PROTECT_H
#define PROTECT_H

/**
  * @file    protect.h
  * @brief   保护类逻辑集中地：protect_init 配置调试外设冻结
  *          （freeze 位）；CSS 受控停机（设计方案 3.2 第 2 条）
  *          待功率阶段落地
  * @author  sdadz-luo
  */

void protect_init(void);

#endif /* PROTECT_H */
