# 阶段四：JSP 当前时间网站

## 1. 项目说明

本项目是 Java 小作业二的 JSP 基础网站。正式源码统一位于
`src/main/webapp`，本目录仅保留部署说明。页面由 Tomcat 服务器执行，
使用 `LocalDateTime.now()` 获取服务器当前时间，并在每次刷新页面时重新计算。

基础时间功能不依赖数据库或 Spring。选做天气页面使用 Java HttpClient
访问 Open-Meteo，并使用 Gson 解析 JSON，不需要申请 API Key。

## 2. 项目目录结构

```text
src/main/
  webapp/
    index.jsp
    weather.jsp
    WEB-INF/
      web.xml
  README.md
```

- `index.jsp`：显示服务器当前时间的主页。
- `weather.jsp`：输入城市并显示当前天气与未来三天预报。
- `WEB-INF/web.xml`：可选的 Web 应用配置，将 `index.jsp` 设置为欢迎页面。
- `README.md`：部署、测试和实验报告说明。

## 3. 页面实现说明

`index.jsp` 使用以下 JSP 配置：

```jsp
<%@ page contentType="text/html; charset=UTF-8" pageEncoding="UTF-8" %>
<%@ page import="java.time.LocalDateTime" %>
<%@ page import="java.time.format.DateTimeFormatter" %>
```

服务器端通过下面的代码生成时间：

```java
LocalDateTime currentTime = LocalDateTime.now();
DateTimeFormatter formatter =
        DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
String formattedTime = currentTime.format(formatter);
```

时间来自运行 Tomcat 的服务器，而不是浏览器本地 JavaScript，也不是写死的文本。

## 4. `web.xml` 是否必须

本项目提供的 `WEB-INF/web.xml` 只配置了欢迎页面：

```xml
<welcome-file-list>
    <welcome-file>index.jsp</welcome-file>
</welcome-file-list>
```

对于普通 JSP 项目，Tomcat 默认通常已经把 `index.jsp` 作为欢迎文件，
因此可以省略 `web.xml`，直接访问：

```text
http://localhost:8080/work2/index.jsp
```

保留 `web.xml` 后，访问 Web 应用根路径即可自动进入主页：

```text
http://localhost:8080/work2/
```

本文件使用 Servlet 3.1 描述符，适合学校机房常见的 Tomcat 8.5 或 Tomcat 9。
对于本项目这种不包含 Servlet Java 类的纯 JSP 页面，在较新 Tomcat 中也不涉及
`javax.servlet` 与 `jakarta.servlet` 源代码包名迁移。

## 5. 部署方式一：直接复制目录

1. 安装与当前 Java 版本兼容的 Apache Tomcat。
2. 关闭正在运行的 Tomcat。
3. 将 `src/main/webapp` 目录复制一份，并将复制后的目录重命名为 `work2`。
4. 把整个 `work2` 目录放入 Tomcat 的 `webapps/` 目录。
5. Windows 下进入 Tomcat 的 `bin` 目录，运行：

   ```bat
   startup.bat
   ```

6. Linux 或 macOS 下运行：

   ```bash
   ./startup.sh
   ```

7. 等待 Tomcat 启动完成，在浏览器访问：

   ```text
   http://localhost:8080/work2/
   ```

如果 8080 端口被修改，应将地址中的 `8080` 换成 Tomcat 实际端口。

## 6. 部署方式二：IDE 部署

### IntelliJ IDEA

1. 创建普通 Java Web Application，或在现有项目中配置 Web Facet。
2. 配置本地 Tomcat Server。
3. 将 `index.jsp` 放入 Web 资源根目录。
4. 将 `WEB-INF/web.xml` 放入对应的 `WEB-INF` 目录。
5. 在 Deployment 中设置应用上下文为 `/work2`。
6. 启动 Tomcat 后访问 `http://localhost:8080/work2/`。

部分 IntelliJ IDEA 版本只有 Ultimate 版内置完整 Java Web 支持。
如果 IDE 不支持，直接复制目录到 `webapps/` 更简单。

### Eclipse

1. 创建 Dynamic Web Project。
2. 将项目名设置为 `work2`。
3. 把 `index.jsp` 放入 `WebContent` 或 `src/main/webapp` 根目录。
4. 把 `web.xml` 放入 `WEB-INF`。
5. 在 Servers 视图中添加 Tomcat，并将项目部署到服务器。
6. 启动后访问 `http://localhost:8080/work2/`。

## 7. 测试与验收步骤

- [ ] Tomcat 能正常启动，控制台没有严重错误。
- [ ] 访问 `http://localhost:8080/work2/` 可以打开页面。
- [ ] 页面标题和中文说明没有乱码。
- [ ] 页面显示格式为 `yyyy-MM-dd HH:mm:ss` 的服务器时间。
- [ ] 等待几秒后刷新页面，时间发生变化。
- [ ] 删除 `web.xml` 后，仍可访问 `/work2/index.jsp`。
- [ ] 保留 `web.xml` 时，访问 `/work2/` 自动进入 `index.jsp`。
- [ ] 不启用天气功能时，当前时间页面仍能独立运行。
- [ ] 点击“查看天气预报（选做）”可进入天气页面。
- [ ] 输入“北京”后能显示城市、当前温度、湿度、风速及未来三天预报。
- [ ] 输入不存在的城市时显示友好错误，不出现 HTTP 500。
- [ ] 断网或接口超时时显示网络错误，当前时间主页仍能使用。

建议同时查看 Tomcat 的 `logs/` 目录。如果页面返回 HTTP 500，
应优先检查 Java 与 Tomcat 版本、JSP 语法和日志中的异常信息。

## 8. 实验报告截图建议

1. Tomcat 启动成功的控制台或日志截图。
2. `work2` Web 应用目录结构截图。
3. 浏览器成功访问 JSP 页面截图。
4. 刷新前后的两张时间变化截图。

## 9. 实验报告简短说明

可以在实验报告中写：

> 本实验使用 Apache Tomcat 作为 Web 服务器，将 JSP 页面部署到
> Tomcat 的 webapps 目录。index.jsp 使用 UTF-8 编码，并通过
> LocalDateTime.now() 在服务器端获取当前时间，再使用
> DateTimeFormatter 将时间格式化为 yyyy-MM-dd HH:mm:ss。
> 浏览器每次刷新页面时，Tomcat 都会重新执行 JSP 中的服务器端代码，
> 因此页面显示的时间会随刷新发生变化。WEB-INF/web.xml 将 index.jsp
> 配置为欢迎页面，使用户访问应用根路径时可以直接打开主页。

如果学校机房使用 Java 7 或更早版本，`java.time` 不可用，可以临时改用：

```java
new java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
        .format(new java.util.Date());
```

主方案仍建议使用 Java 8 及以上版本的 `java.time`。

## 10. 天气预报选做模块

天气功能已经完成，处理流程如下：

1. `weather.jsp` 接收用户输入的城市名称。
2. `WeatherService` 调用 Open-Meteo Geocoding API 获取经纬度。
3. 再调用 Forecast API 获取当前天气和未来三天预报。
4. Gson 将 JSON 转换成 Java 数据对象。
5. JSP 展示温度、体感温度、湿度、风速、天气状况和逐日预报。

关键设计：

- 无需 API Key，避免在源码中保存密钥。
- HTTP 连接和请求均设置超时。
- 城市不存在、断网、接口错误和 JSON 异常都有友好提示。
- 用户输入和错误文本在输出前进行 HTML 转义，避免直接输出不可信内容。
- 天气失败不会影响 `index.jsp` 当前时间基础功能。
