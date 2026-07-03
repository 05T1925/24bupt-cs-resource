// =============================================================================
// ExpressItem.cpp - 多态计费公式实现
// =============================================================================
// LogisticsSystem::sendExpress 通过工厂获得基类指针，再调用 getPrice。
// 因此新增物品类型时主要扩展本模块，无需在寄件流程中复制计费分支。
// =============================================================================

#include "ExpressItem.h"

#include "../security/StringUtil.h"

// 三种派生类型封装各自计价公式，使寄件业务只依赖统一的 getPrice 接口。
NormalItem::NormalItem(double weightKg) : weightKg_(weightKg) {}

std::string NormalItem::typeCode() const {
    return "Normal";
}

std::string NormalItem::typeName() const {
    return "普通快递";
}

std::string NormalItem::amountText() const {
    return StringUtil::formatAmount(weightKg_) + " kg";
}

double NormalItem::getPrice() const {
    return weightKg_ * 5.0;
}

FragileItem::FragileItem(double weightKg) : weightKg_(weightKg) {}

std::string FragileItem::typeCode() const {
    return "Fragile";
}

std::string FragileItem::typeName() const {
    return "易碎品";
}

std::string FragileItem::amountText() const {
    return StringUtil::formatAmount(weightKg_) + " kg";
}

double FragileItem::getPrice() const {
    return weightKg_ * 8.0;
}

BookItem::BookItem(double count) : count_(count) {}

std::string BookItem::typeCode() const {
    return "Book";
}

std::string BookItem::typeName() const {
    return "图书";
}

std::string BookItem::amountText() const {
    return StringUtil::formatAmount(count_) + " 本";
}

double BookItem::getPrice() const {
    return count_ * 2.0;
}

std::unique_ptr<ExpressItem> ExpressItemFactory::createItem(const std::string& type, double amount) {
    // 工厂同时承担类型白名单与正数校验，nullptr 表示无法构造合法计价对象。
    if (amount <= 0.0) {
        return nullptr;
    }
    if (type == "Normal") {
        return std::unique_ptr<ExpressItem>(new NormalItem(amount));
    }
    if (type == "Fragile") {
        return std::unique_ptr<ExpressItem>(new FragileItem(amount));
    }
    if (type == "Book") {
        return std::unique_ptr<ExpressItem>(new BookItem(amount));
    }
    return nullptr;
}
