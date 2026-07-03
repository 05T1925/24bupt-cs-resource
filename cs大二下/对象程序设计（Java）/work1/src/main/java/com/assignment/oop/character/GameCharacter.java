package com.assignment.oop.character;

/**
 * Q3 · GameCharacter 抽象基类
 *
 * 定义所有游戏角色的共性：
 *   - 名称（name）：角色名称
 *   - 等级（level）：角色等级
 *   - 抽象方法 attack()：由子类各自实现不同的攻击方式
 *
 * 设计意图：与 Q2 的 Animal 类似，但增加了更多属性（等级），
 *   并在测试类中使用 ArrayList 进行动态管理，引入 instanceof 安全转型。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   与 Animal 模式相同——父类 struct 定义公共字段 + 虚函数表
 *   typedef struct {
 *       void (*attack)(void* this_);  // 纯虚函数
 *       char* name;
 *       int   level;
 *   } GameCharacter;
 *
 * 【与 Q2 Animal 的区别】
 *   - 多了 level 字段 → 堆上对象多 4 字节（int）
 *   - 使用 ArrayList 管理 → 动态数组（类似 C++ std::vector，C 中需手动 realloc）
 *   - instanceof + 向下转型 → 这是 Q2 没有的新知识点
 *
 * @author JavaHomework
 * @version 1.0
 */
public abstract class GameCharacter {
    // abstract class → 类访问标志含 ACC_ABSTRACT
    // new 指令拒绝为此类分配对象（JVM 级别检查）

    private String name;
    private int level;

    /**
     * 构造器：创建游戏角色时必须指定名称和等级
     *
     * @param name  角色名称
     * @param level 角色等级
     */
    public GameCharacter(String name, int level) {
        this.name = name;
        this.level = level;
    }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }
    public int getLevel() { return level; }
    public void setLevel(int level) { this.level = level; }

    /**
     * 抽象方法：攻击
     * 每个子类必须实现自己独特的攻击方式
     *
     * 【底层】在 vtable 中留一个槽位（初始为 NULL）
     * 编译器强制子类必须重写 → JVM 类加载时验证 vtable 无 NULL 虚函数槽位
     */
    public abstract void attack();

    /**
     * 获取角色种类名称
     *
     * 【底层】通过对象头中的 Klass Pointer 访问 Metaspace 中的类元数据
     * getClass() 返回 Klass 关联的 java.lang.Class 镜像对象
     * 等价于 C: return this_->klass->class_name;
     */
    public String getSpecies() {
        return this.getClass().getSimpleName();
    }
}
