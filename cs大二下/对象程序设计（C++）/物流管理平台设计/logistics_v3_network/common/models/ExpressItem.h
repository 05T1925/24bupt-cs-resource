// =============================================================================
// ExpressItem.h — 多态计费模型
// =============================================================================
// 文件用途：定义快递物品的多态计费体系，不同物品类型有不同的计费规则。
// 所属模块：common/models（纯业务模型层）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 计费规则（由服务端 LogisticsSystem::sendExpress 调用，客户端不传费用）：
//   NormalItem  (普通快递)：5 元/kg  × weightKg
//   FragileItem (易碎品)：  8 元/kg  × weightKg
//   BookItem    (图书)：    2 元/本  × count
//
// 设计红线：
//   - 客户端只提交物品类型(typeCode)和数量(amount)，费用必须由服务端计算
//   - ExpressItemFactory::createItem 在无效类型/数量时返回 nullptr
//   - 所有派生类通过 getPrice() 返回最终费用
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_MODELS_EXPRESS_ITEM_H
#define LOGISTICS_V3_COMMON_MODELS_EXPRESS_ITEM_H

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// ExpressItem — 快递物品抽象基类（多态计费）
// ---------------------------------------------------------------------------
class ExpressItem {
public:
    virtual ~ExpressItem() = default;
    virtual std::string typeCode() const = 0;    // 类型代码：Normal / Fragile / Book
    virtual std::string typeName() const = 0;    // 中文名称：普通快递 / 易碎品 / 图书
    virtual std::string amountText() const = 0;  // 数量单位：重量(kg) / 册数
    virtual double getPrice() const = 0;         // 计算最终费用（由服务端调用）
};

// ---------------------------------------------------------------------------
// NormalItem — 普通快递：5 元/kg
// ---------------------------------------------------------------------------
class NormalItem final : public ExpressItem {
public:
    explicit NormalItem(double weightKg);
    std::string typeCode() const override;    // "Normal"
    std::string typeName() const override;    // "普通快递"
    std::string amountText() const override;
    double getPrice() const override;         // 5.0 * weightKg_

private:
    double weightKg_;
};

// ---------------------------------------------------------------------------
// FragileItem — 易碎品：8 元/kg
// ---------------------------------------------------------------------------
class FragileItem final : public ExpressItem {
public:
    explicit FragileItem(double weightKg);
    std::string typeCode() const override;    // "Fragile"
    std::string typeName() const override;    // "易碎品"
    std::string amountText() const override;
    double getPrice() const override;         // 8.0 * weightKg_

private:
    double weightKg_;
};

// ---------------------------------------------------------------------------
// BookItem — 图书：2 元/本
// ---------------------------------------------------------------------------
class BookItem final : public ExpressItem {
public:
    explicit BookItem(double count);
    std::string typeCode() const override;    // "Book"
    std::string typeName() const override;    // "图书"
    std::string amountText() const override;
    double getPrice() const override;         // 2.0 * count_

private:
    double count_;
};

// ---------------------------------------------------------------------------
// ExpressItemFactory — 快递物品工厂
// 根据 type 字符串和 amount 创建对应的 ExpressItem 派生类实例
// "Normal" → NormalItem, "Fragile" → FragileItem, "Book" → BookItem
// 无效类型或 amount<=0 时返回 nullptr（由调用者检查）
// ---------------------------------------------------------------------------
class ExpressItemFactory {
public:
    static std::unique_ptr<ExpressItem> createItem(const std::string& type, double amount);
};

#endif
