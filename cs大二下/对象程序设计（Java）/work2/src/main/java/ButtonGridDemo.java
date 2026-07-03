import java.awt.Color;
import java.awt.EventQueue;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.event.ActionListener;

import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.WindowConstants;

/**
 * 小作业二第 6 题：使用 GridBagLayout 实现指定按钮布局和事件处理。
 *
 * 交互行为：
 *   1. 点击 5：按钮 6 的文字变为空白。
 *   2. 点击 7：按钮 8 增加一个空白文本行。
 *   3. 连续点击 9：按钮 10 在禁用和启用状态之间切换。
 *   4. 点击 10：按钮 15 红底绿字。
 *   5. 点击 11：按钮 15 蓝底红字。
 *   6. 按钮 10 和 11 共用 colorListener，通过事件源区分来源。
 */
public class ButtonGridDemo extends JFrame {
    // 使用 1~19 下标对应按钮编号；下标 0 留空，使代码与题图编号一致。
    private final JButton[] buttons = new JButton[20];
    // 左上方题图中的大块空白区域使用一个 JPanel 占位。
    private final JPanel blankPanel = new JPanel();

    public ButtonGridDemo() {
        setTitle("Button Grid Demo");
        setSize(850, 620);
        // null 表示相对于屏幕居中显示窗口。
        setLocationRelativeTo(null);
        // 关闭窗口时结束 Java 进程，避免事件派发线程继续运行。
        setDefaultCloseOperation(WindowConstants.EXIT_ON_CLOSE);

        initButtons();
        buildLayout();
        bindEvents();
    }

    public static void main(String[] args) {
        // Swing 组件应在事件派发线程（EDT）中创建和显示。
        EventQueue.invokeLater(new Runnable() {
            @Override
            public void run() {
                new ButtonGridDemo().setVisible(true);
            }
        });
    }

    /**
     * 统一创建按钮，数组下标 1 到 19 分别对应按钮 1 到 19。
     */
    private void initButtons() {
        // 数组下标直接对应按钮编号，事件处理中可以按编号访问目标按钮。
        for (int i = 1; i <= 19; i++) {
            buttons[i] = new JButton(String.valueOf(i));
            buttons[i].setFocusPainted(false);
        }

        // 修改按钮 15 背景色时，部分 LookAndFeel 需要这些设置才会明显生效。
        buttons[15].setOpaque(true);
        buttons[15].setBorderPainted(false);
    }

    /**
     * 使用 GridBagLayout 组装界面。
     *
     * 整体网格说明：
     * 1. 左侧使用 gridx=0 到 4，共 5 列。
     * 2. 右侧使用 gridx=5 到 7，共 3 列。
     * 3. 上方使用 gridy=0 到 7，共 8 行。
     * 4. 底部使用 gridy=8 到 9，共 2 行。
     *
     * 关键组件约束：
     * - 左上空白区域：gridx=0, gridy=0, gridwidth=5, gridheight=8,
     *   weightx=5.0, weighty=8.0，占据左上方大面积空间。
     * - 按钮 1：gridx=5, gridy=0, gridwidth=3, gridheight=1。
     * - 按钮 2、3、4：gridx=5/6/7, gridy=1, 各占 1 列。
     * - 按钮 5、6、10、11、12、18、19：右侧整行按钮，
     *   gridx=5, gridwidth=3。
     * - 按钮 7、8、9：gridx=5/6/7, gridy=4, 各占 1 列。
     * - 按钮 13 到 17：位于底部，gridy=8, gridheight=2，
     *   在左侧 5 列横向排列。
     */
    private void buildLayout() {
        setLayout(new GridBagLayout());

        blankPanel.setBackground(Color.BLACK);
        add(blankPanel, createConstraints(0, 0, 5, 8, 5.0, 8.0));

        add(buttons[1], createConstraints(5, 0, 3, 1, 3.0, 1.0));
        add(buttons[2], createConstraints(5, 1, 1, 1, 1.0, 1.0));
        add(buttons[3], createConstraints(6, 1, 1, 1, 1.0, 1.0));
        add(buttons[4], createConstraints(7, 1, 1, 1, 1.0, 1.0));

        add(buttons[5], createConstraints(5, 2, 3, 1, 3.0, 1.0));
        add(buttons[6], createConstraints(5, 3, 3, 1, 3.0, 1.0));

        add(buttons[7], createConstraints(5, 4, 1, 1, 1.0, 1.0));
        add(buttons[8], createConstraints(6, 4, 1, 1, 1.0, 1.0));
        add(buttons[9], createConstraints(7, 4, 1, 1, 1.0, 1.0));

        add(buttons[10], createConstraints(5, 5, 3, 1, 3.0, 1.0));
        add(buttons[11], createConstraints(5, 6, 3, 1, 3.0, 1.0));
        add(buttons[12], createConstraints(5, 7, 3, 1, 3.0, 1.0));

        add(buttons[13], createConstraints(0, 8, 1, 2, 1.0, 2.0));
        add(buttons[14], createConstraints(1, 8, 1, 2, 1.0, 2.0));
        add(buttons[15], createConstraints(2, 8, 1, 2, 1.0, 2.0));
        add(buttons[16], createConstraints(3, 8, 1, 2, 1.0, 2.0));
        add(buttons[17], createConstraints(4, 8, 1, 2, 1.0, 2.0));

        add(buttons[18], createConstraints(5, 8, 3, 1, 3.0, 1.0));
        add(buttons[19], createConstraints(5, 9, 3, 1, 3.0, 1.0));
    }

    /**
     * 统一创建 GridBagConstraints，避免重复代码。
     */
    private GridBagConstraints createConstraints(int gridx, int gridy,
                                                 int gridwidth, int gridheight,
                                                 double weightx, double weighty) {
        GridBagConstraints gbc = new GridBagConstraints();
        // gridx/gridy 指定网格位置，gridwidth/gridheight 指定组件跨越的格数。
        gbc.gridx = gridx;
        gbc.gridy = gridy;
        gbc.gridwidth = gridwidth;
        gbc.gridheight = gridheight;
        gbc.weightx = weightx;
        gbc.weighty = weighty;
        // BOTH 表示组件同时填满分配到的水平和垂直空间。
        gbc.fill = GridBagConstraints.BOTH;
        // 每个组件四周保留 1 像素间距，使按钮边界更容易观察。
        gbc.insets = new Insets(1, 1, 1, 1);
        return gbc;
    }

    /**
     * 单独绑定事件，便于和界面布局代码区分。
     */
    private void bindEvents() {
        // 按钮 5：清空按钮 6 的文字。
        buttons[5].addActionListener(e -> {
            buttons[6].setText("");
            refreshButton(buttons[6]);
        });

        // 按钮 7：使用 HTML 换行，在按钮 8 中增加一个空白文本行。
        buttons[7].addActionListener(e -> {
            buttons[8].setText("<html>8<br>&nbsp;</html>");
            refreshButton(buttons[8]);
        });

        // 按钮 9：在按钮 10 的可用和不可用状态之间切换。
        buttons[9].addActionListener(e -> {
            // 取当前状态的反值，所以连续点击可以在启用/禁用之间切换。
            buttons[10].setEnabled(!buttons[10].isEnabled());
            refreshButton(buttons[10]);
        });

        // 按钮 10 和按钮 11 强制共享同一个事件处理器，通过 e.getSource() 判断来源。
        ActionListener colorListener = e -> {
            Object source = e.getSource();

            // 事件源就是触发监听器的组件，可据此让同一监听器处理两个按钮。
            if (source == buttons[10]) {
                buttons[15].setBackground(Color.RED);
                buttons[15].setForeground(Color.GREEN);
                refreshButton(buttons[15]);
            } else if (source == buttons[11]) {
                buttons[15].setBackground(Color.BLUE);
                buttons[15].setForeground(Color.RED);
                refreshButton(buttons[15]);
            }
        };

        buttons[10].addActionListener(colorListener);
        buttons[11].addActionListener(colorListener);
    }

    /**
     * 动态修改按钮外观或文本后，显式刷新组件。
     */
    private void refreshButton(JButton button) {
        button.revalidate();
        button.repaint();
    }
}
