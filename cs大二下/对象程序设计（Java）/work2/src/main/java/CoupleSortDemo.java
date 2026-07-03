/**
 * 小作业二第 7 题：泛型组合排序演示。
 *
 * arra[i] 和 arrb[i] 共同组成一个组合。排序时必须同步交换两个数组中的元素，
 * 从而保证排序后同一下标的两个对象仍然保持原来的组合关系。
 *
 * 泛型参数 A、B 表示任意两种对象类型，CoupleComparator<A, B> 由调用者
 * 提供组合大小规则，因此排序算法不依赖示例中的 Student 和 Score。
 *
 * 运行结果：
 *   排序前后每个学生仍与原来的成绩对象配对；输出顺序符合比较器中
 *   “Java 成绩降序、数学成绩降序、年龄升序、姓名升序”的规则。
 */
public class CoupleSortDemo {

    /**
     * 比较两个由 A、B 对象组成的组合。
     *
     * 返回负数表示第一个组合较小，返回 0 表示相等，返回正数表示第一个组合较大。
     * 比较规则由调用者传入，因此排序方法不依赖 Student、Score 等具体类型。
     */
    interface CoupleComparator<A, B> {
        /*
         * 约定与 Comparator 相同：负数表示第一个组合排前，0 表示等价，
         * 正数表示第二个组合排前。
         */
        int compare(A a1, B b1, A a2, B b2);
    }

    /**
     * 使用外部传入的比较器比较两个组合。
     */
    public static <A, B> int compareCouple(
            A a1, B b1,
            A a2, B b2,
            CoupleComparator<A, B> comparator) {

        if (comparator == null) {
            throw new IllegalArgumentException("组合比较器 comparator 不能为 null。");
        }

        return comparator.compare(a1, b1, a2, b2);
    }

    /**
     * 按比较器规定的顺序对两个数组构成的组合进行选择排序。
     * 不能分别排序两个数组，否则同一下标对象之间的对应关系会被破坏。
     */
    public static <A, B> void sortCouple(
            A[] arra,
            B[] arrb,
            CoupleComparator<A, B> comparator) {

        validateArguments(arra, arrb, comparator);

        for (int i = 0; i < arra.length - 1; i++) {
            /*
             * 选择排序：位置 i 左边已经有序，本轮从 i~末尾寻找最应该放到 i
             * 的组合。算法原地排序，额外空间为 O(1)，比较次数为 O(n²)。
             */
            int selectedIndex = i;

            for (int j = i + 1; j < arra.length; j++) {
                if (compareCouple(
                        arra[j], arrb[j],
                        arra[selectedIndex], arrb[selectedIndex],
                        comparator) < 0) {
                    // 找到比当前候选组合更靠前的组合，记录它的下标。
                    selectedIndex = j;
                }
            }

            if (selectedIndex != i) {
                // 两个数组必须同步交换，否则学生和成绩的对应关系会被打乱。
                A tempA = arra[i];
                arra[i] = arra[selectedIndex];
                arra[selectedIndex] = tempA;

                B tempB = arrb[i];
                arrb[i] = arrb[selectedIndex];
                arrb[selectedIndex] = tempB;
            }
        }
    }

    /**
     * 保留小写方法名，作为 compareCouple 的兼容入口。
     */
    public static <A, B> int comparecouple(
            A a1, B b1,
            A a2, B b2,
            CoupleComparator<A, B> comparator) {

        return compareCouple(a1, b1, a2, b2, comparator);
    }

    /**
     * 保留小写方法名，作为 sortCouple 的兼容入口。
     */
    public static <A, B> void sortcouple(
            A[] arra,
            B[] arrb,
            CoupleComparator<A, B> comparator) {

        sortCouple(arra, arrb, comparator);
    }

    /**
     * 集中检查排序方法的参数。
     */
    private static <A, B> void validateArguments(
            A[] arra,
            B[] arrb,
            CoupleComparator<A, B> comparator) {

        if (arra == null) {
            throw new IllegalArgumentException("A 对象数组 arra 不能为 null。");
        }
        if (arrb == null) {
            throw new IllegalArgumentException("B 对象数组 arrb 不能为 null。");
        }
        if (comparator == null) {
            throw new IllegalArgumentException("组合比较器 comparator 不能为 null。");
        }
        if (arra.length != arrb.length) {
            throw new IllegalArgumentException(
                    "arra 和 arrb 的长度必须相同，当前长度分别为 "
                            + arra.length + " 和 " + arrb.length + "。");
        }
    }

    /**
     * 打印两个数组中同一下标的组合。
     */
    private static <A, B> void printCouples(A[] arra, B[] arrb) {
        if (arra == null || arrb == null) {
            throw new IllegalArgumentException("打印时，arra 和 arrb 都不能为 null。");
        }
        if (arra.length != arrb.length) {
            throw new IllegalArgumentException("打印时，arra 和 arrb 的长度必须相同。");
        }

        for (int i = 0; i < arra.length; i++) {
            System.out.println("[" + i + "] " + arra[i] + " + " + arrb[i]);
        }
    }

    public static void main(String[] args) {
        // students[i] 必须始终与 scores[i] 作为一个整体参与比较和交换。
        Student[] students = {
            new Student("Alice", 20),
            new Student("Bob", 19),
            new Student("Cindy", 21),
            new Student("David", 20),
            new Student("Eva", 18),
            new Student("Aaron", 18)
        };

        Score[] scores = {
            new Score(85, 90),
            new Score(92, 88),
            new Score(85, 95),
            new Score(92, 91),
            new Score(85, 95),
            new Score(85, 95)
        };

        /*
         * 比较规则不写死在排序算法中，而由调用者通过泛型比较器提供。
         * 因此同一个 sortCouple 方法也能排序其它两种类型构成的组合。
         */
        CoupleComparator<Student, Score> comparator =
                new CoupleComparator<Student, Score>() {
                    @Override
                    public int compare(
                            Student student1, Score score1,
                            Student student2, Score score2) {

                        // Java 成绩降序：分数高的组合应排在前面。
                        int result = Integer.compare(score2.getJavaScore(), score1.getJavaScore());
                        if (result != 0) {
                            return result;
                        }

                        // Java 成绩相同，则按数学成绩降序。
                        result = Integer.compare(score2.getMathScore(), score1.getMathScore());
                        if (result != 0) {
                            return result;
                        }

                        // 两科成绩都相同，则按年龄升序。
                        result = Integer.compare(student1.getAge(), student2.getAge());
                        if (result != 0) {
                            return result;
                        }

                        // 年龄仍相同，则按姓名字典序升序。
                        return student1.getName().compareTo(student2.getName());
                    }
                };

        System.out.println("排序前：");
        printCouples(students, scores);

        // 使用小写兼容入口调用，内部会转调标准驼峰命名方法。
        sortcouple(students, scores, comparator);

        System.out.println();
        System.out.println("排序后：");
        printCouples(students, scores);
    }

    /**
     * 示例类 A：学生基本信息，只用于测试泛型组合排序。
     */
    static class Student {
        private final String name;
        private final int age;

        Student(String name, int age) {
            this.name = name;
            this.age = age;
        }

        String getName() {
            return name;
        }

        int getAge() {
            return age;
        }

        @Override
        public String toString() {
            return "Student{name='" + name + "', age=" + age + "}";
        }
    }

    /**
     * 示例类 B：学生成绩，只用于测试泛型组合排序。
     */
    static class Score {
        private final int javaScore;
        private final int mathScore;

        Score(int javaScore, int mathScore) {
            this.javaScore = javaScore;
            this.mathScore = mathScore;
        }

        int getJavaScore() {
            return javaScore;
        }

        int getMathScore() {
            return mathScore;
        }

        @Override
        public String toString() {
            return "Score{javaScore=" + javaScore
                    + ", mathScore=" + mathScore + "}";
        }
    }
}
