import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.BasicFileAttributes;

/**
 * 小作业二第 4 题：使用 Java NIO.2 实现类似 DOS xcopy 的目录递归复制工具。
 *
 * 核心知识点：
 *   walkFileTree() 负责递归遍历；preVisitDirectory() 创建目标目录；
 *   visitFile() 复制普通文件。通过 relativize() 和 resolve() 保留原目录层级。
 *
 * 功能说明：
 *   目标目录可以事先不存在；复制后普通文件、子目录和空目录都应保留。
 *   程序还会拒绝源目录等于目标目录，或目标目录位于源目录内部的情况。
 *
 * 运行示例：
 *   javac XCopy.java
 *   java XCopy source_dir target_dir
 *
 * 含义：
 *   将 source_dir 目录下面的所有子目录和文件复制到 target_dir 目录下面。
 *   如果 target_dir 不存在，程序会自动创建。
 */
public class XCopy {
    public static void main(String[] args) {
        // 两个命令行参数分别表示源目录 dir1 和目标目录 dir2。
        if (args.length != 2) {
            System.out.println("参数数量错误！");
            System.out.println("用法: java XCopy <源目录dir1> <目标目录dir2>");
            System.out.println("示例: java XCopy source_dir target_dir");
            return;
        }

        /*
         * 转为绝对规范路径便于比较。normalize() 会消除路径中的 "." 和可化简的
         * ".."，避免同一目录因写法不同而绕过相等或包含关系检查。
         */
        Path sourceDir = Paths.get(args[0]).toAbsolutePath().normalize();
        Path targetDir = Paths.get(args[1]).toAbsolutePath().normalize();

        if (!Files.exists(sourceDir)) {
            System.out.println("源目录不存在: " + sourceDir);
            return;
        }

        if (!Files.isDirectory(sourceDir)) {
            System.out.println("源路径不是目录: " + sourceDir);
            return;
        }

        if (Files.exists(targetDir) && !Files.isDirectory(targetDir)) {
            System.out.println("目标路径已存在，但不是目录: " + targetDir);
            return;
        }

        if (sourceDir.equals(targetDir)) {
            System.out.println("源目录和目标目录不能相同: " + sourceDir);
            return;
        }

        // 避免把目标目录放在源目录内部，否则遍历时可能不断复制新生成的文件。
        if (targetDir.startsWith(sourceDir)) {
            System.out.println("目标目录不能位于源目录内部: " + targetDir);
            return;
        }

        try {
            // walkFileTree 会递归访问源目录中的目录和文件，并保持原有层级。
            Files.walkFileTree(sourceDir, new CopyFileVisitor(sourceDir, targetDir));
            System.out.println("目录复制完成！");
            System.out.println("源目录: " + sourceDir);
            System.out.println("目标目录: " + targetDir);
        } catch (FileAlreadyExistsException e) {
            System.out.println("复制失败，目标文件已存在且无法覆盖: " + e.getFile());
        } catch (IOException e) {
            System.out.println("复制过程中发生 I/O 错误: " + e.getMessage());
        }
    }

    /**
     * 自定义文件访问器，在遍历源目录树时同步创建目录并复制文件。
     */
    private static class CopyFileVisitor extends SimpleFileVisitor<Path> {
        private final Path sourceDir;
        private final Path targetDir;

        CopyFileVisitor(Path sourceDir, Path targetDir) {
            this.sourceDir = sourceDir;
            this.targetDir = targetDir;
        }

        @Override
        public FileVisitResult preVisitDirectory(Path dir, BasicFileAttributes attrs) throws IOException {
            /*
             * 例如源目录为 C:\source，当前目录为 C:\source\images：
             * relativize 得到 images，resolve 后得到 C:\target\images。
             */
            Path relativePath = sourceDir.relativize(dir);
            Path targetPath = targetDir.resolve(relativePath);

            // 进入源目录时先建立对应目标目录，空目录也能被保留下来。
            if (!Files.exists(targetPath)) {
                Files.createDirectories(targetPath);
                System.out.println("创建目录: " + targetPath);
            } else if (!Files.isDirectory(targetPath)) {
                throw new FileAlreadyExistsException(targetPath.toString(),
                        null, "目标位置已存在同名文件，无法创建目录");
            }

            // 告诉 walkFileTree 继续进入当前目录并访问其子项。
            return FileVisitResult.CONTINUE;
        }

        @Override
        public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
            Path relativePath = sourceDir.relativize(file);
            Path targetPath = targetDir.resolve(relativePath);

            // 目标存在同名文件时覆盖，并尽量复制原文件属性。
            Files.copy(file, targetPath,
                    // 同名目标文件存在时覆盖，而不是直接抛出异常。
                    StandardCopyOption.REPLACE_EXISTING,
                    // 尽量保留修改时间等基础文件属性。
                    StandardCopyOption.COPY_ATTRIBUTES);
            System.out.println("复制文件: " + file + " -> " + targetPath);

            return FileVisitResult.CONTINUE;
        }

        @Override
        public FileVisitResult visitFileFailed(Path file, IOException exc) throws IOException {
            System.out.println("访问文件失败: " + file + "，原因: " + exc.getMessage());
            throw exc;
        }
    }
}
