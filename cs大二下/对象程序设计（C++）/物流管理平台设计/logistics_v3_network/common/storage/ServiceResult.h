// =============================================================================
// ServiceResult.h — 统一业务返回结构
// =============================================================================
// 文件用途：定义 LogisticsSystem 全部业务方法的标准返回值类型。
// 所属模块：common/service（业务服务层）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 字段说明：
//   ok      — 业务操作是否成功（true=成功, false=失败）
//   code    — 标准结果码（如 SUCCESS / PERMISSION_DENIED / STATE_CONFLICT 等）
//   message — 人类可读的消息描述（客户端可直接展示）
//   data    — 附加数据列表（如快递列表、绩效记录、金额信息等）
//
// 工厂方法：
//   success(code, message)           — 创建成功结果（无附加数据）
//   successWithData(code, msg, data) — 创建成功结果（含附加数据）
//   failure(code, message)           — 创建失败结果
//
// 使用范例：
//   return ServiceResult::success("SUCCESS", "寄件成功。");
//   return ServiceResult::failure("PERMISSION_DENIED", "无权执行此操作。");
//   return ServiceResult::successWithData("SUCCESS", "查询成功。", records);
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_SERVICE_SERVICE_RESULT_H
#define LOGISTICS_V3_COMMON_SERVICE_SERVICE_RESULT_H

#include <string>
#include <vector>

struct ServiceResult {
    bool ok = false;                    // 操作是否成功
    std::string code;                   // 标准结果码
    std::string message;                // 可读消息
    std::vector<std::string> data;      // 附加数据列表

    // 创建成功结果（无附加数据）
    static ServiceResult success(const std::string& code, const std::string& message) {
        ServiceResult result;
        result.ok = true;
        result.code = code;
        result.message = message;
        return result;
    }

    // 创建成功结果（含附加数据，如快递列表、绩效记录）
    static ServiceResult successWithData(const std::string& code, const std::string& message,
                                         const std::vector<std::string>& data) {
        ServiceResult result = success(code, message);
        result.data = data;
        return result;
    }

    // 创建失败结果
    static ServiceResult failure(const std::string& code, const std::string& message) {
        ServiceResult result;
        result.ok = false;
        result.code = code;
        result.message = message;
        return result;
    }
};

#endif
