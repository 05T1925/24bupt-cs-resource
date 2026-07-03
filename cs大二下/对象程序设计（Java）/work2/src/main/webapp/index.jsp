<%-- 小作业二第 5 题：JSP 网站显示服务器当前时间。 --%>
<%-- JSP 和响应都使用 UTF-8，避免页面中的中文出现乱码。 --%>
<%@ page contentType="text/html; charset=UTF-8" pageEncoding="UTF-8" %>
<%@ page import="java.time.LocalDateTime" %>
<%@ page import="java.time.format.DateTimeFormatter" %>
<%--
    这段代码在 Tomcat 服务器端执行，而不是在浏览器中运行。
    每次刷新页面时服务器都会重新计算时间，因此页面内容不是静态文本。
--%>
<%
    // LocalDateTime.now() 在 JSP 被请求时执行，得到的是服务器当前时间。
    LocalDateTime currentTime = LocalDateTime.now();
    // DateTimeFormatter 只负责显示格式，不改变时间本身。
    DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
    String formattedTime = currentTime.format(formatter);
%>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Java 小作业二：JSP 当前时间</title>
    <style>
        body {
            margin: 0;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #202124;
            background-color: #f4f6f8;
            font-family: Arial, "Microsoft YaHei", sans-serif;
        }

        main {
            width: min(560px, calc(100% - 40px));
            padding: 32px;
            border: 1px solid #d8dde3;
            border-radius: 8px;
            background-color: #ffffff;
            box-shadow: 0 6px 20px rgba(32, 33, 36, 0.08);
        }

        h1 {
            margin: 0 0 24px;
            font-size: 26px;
            letter-spacing: 0;
        }

        .time-label {
            margin-bottom: 8px;
            color: #5f6368;
        }

        .time-value {
            margin: 0 0 24px;
            color: #0b57d0;
            font-size: 32px;
            font-weight: bold;
            letter-spacing: 0;
        }

        .description {
            margin: 0 0 12px;
            line-height: 1.7;
        }

        .refresh-tip {
            margin: 0 0 22px;
            color: #5f6368;
            font-size: 14px;
        }

        .weather-link {
            display: inline-block;
            padding: 11px 16px;
            border-radius: 8px;
            color: #ffffff;
            background-color: #16805b;
            font-weight: bold;
            text-decoration: none;
        }
    </style>
</head>
<body>
<main>
    <h1>Java 小作业二：JSP 当前时间</h1>

    <div class="time-label">当前服务器时间</div>
    <%-- JSP 表达式 <%= ... %> 会把服务器端变量值写入最终 HTML 响应。 --%>
    <div class="time-value"><%= formattedTime %></div>

    <p class="description">
        该时间由服务器端 JSP 动态生成，使用 LocalDateTime 获取并格式化。
    </p>
    <p class="refresh-tip">刷新页面后，服务器会重新计算并显示当前时间。</p>
    <a class="weather-link" href="weather.jsp">查看天气预报（选做）</a>
</main>
</body>
</html>
