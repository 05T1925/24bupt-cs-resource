<%-- 小作业二第 5 题选做：连接天气服务并显示预报。 --%>
<%@ page contentType="text/html; charset=UTF-8" pageEncoding="UTF-8" %>
<%@ page import="cn.edu.work2.weather.WeatherService" %>
<%@ page import="cn.edu.work2.weather.WeatherService.DailyForecast" %>
<%@ page import="cn.edu.work2.weather.WeatherService.WeatherReport" %>
<%@ page import="cn.edu.work2.weather.WeatherService.WeatherException" %>
<%!
    /*
     * 用户输入、城市名称和接口错误都不是页面自身的固定文本。
     * 输出前转义 HTML 特殊字符，避免输入内容被浏览器当成标签执行。
     */
    private static String html(String value) {
        if (value == null) {
            return "";
        }
        return value.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace("\"", "&quot;")
                .replace("'", "&#39;");
    }
%>
<%
    // GET 查询参数使用 UTF-8 解码，以便正确接收中文城市名。
    request.setCharacterEncoding("UTF-8");

    // 表单使用 GET 方法，因此城市名来自 URL 查询参数 city。
    String city = request.getParameter("city");
    String submittedCity = city == null ? "" : city.trim();
    WeatherReport weather = null;
    String errorMessage = null;

    if (!submittedCity.isEmpty()) {
        try {
            // JSP 负责接收参数和展示，具体 HTTP 请求与 JSON 解析交给 WeatherService。
            weather = new WeatherService().getWeather(submittedCity);
        } catch (WeatherException e) {
            // 预期内的城市、网络和数据错误转为页面提示，不让用户看到服务器异常堆栈。
            errorMessage = e.getMessage();
        }
    }
%>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>天气预报选做功能</title>
    <style>
        * { box-sizing: border-box; }
        body {
            margin: 0;
            min-height: 100vh;
            color: #172033;
            background: linear-gradient(145deg, #e8f3ff, #f7fbff 55%, #edf7f2);
            font-family: Arial, "Microsoft YaHei", sans-serif;
        }
        main {
            width: min(920px, calc(100% - 32px));
            margin: 42px auto;
        }
        .panel {
            padding: 28px;
            border: 1px solid rgba(54, 92, 130, 0.18);
            border-radius: 18px;
            background: rgba(255, 255, 255, 0.94);
            box-shadow: 0 16px 48px rgba(43, 76, 110, 0.12);
        }
        .topbar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 16px;
            margin-bottom: 24px;
        }
        h1, h2, p { margin-top: 0; }
        h1 { margin-bottom: 8px; font-size: 28px; }
        .subtitle { margin-bottom: 0; color: #607086; }
        .back {
            color: #1769aa;
            font-weight: bold;
            text-decoration: none;
        }
        form {
            display: flex;
            gap: 10px;
            margin-bottom: 24px;
        }
        input {
            flex: 1;
            min-width: 0;
            padding: 12px 14px;
            border: 1px solid #b9c9d8;
            border-radius: 10px;
            font: inherit;
        }
        button {
            padding: 12px 22px;
            border: 0;
            border-radius: 10px;
            color: #fff;
            background: #1769aa;
            font: inherit;
            font-weight: bold;
            cursor: pointer;
        }
        .notice, .error {
            padding: 14px 16px;
            border-radius: 10px;
            line-height: 1.6;
        }
        .notice { color: #42556c; background: #f1f6fa; }
        .error { color: #8b2525; background: #fff0f0; }
        .current {
            display: grid;
            grid-template-columns: 1.3fr repeat(3, 1fr);
            gap: 14px;
            margin-bottom: 24px;
        }
        .metric, .day-card {
            padding: 18px;
            border: 1px solid #d9e3ec;
            border-radius: 14px;
            background: #fff;
        }
        .metric-label { color: #68788b; font-size: 14px; }
        .metric-value { margin-top: 7px; font-size: 24px; font-weight: bold; }
        .temperature { color: #d95f32; font-size: 38px; }
        .forecast-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 14px;
        }
        .day-card h3 { margin: 0 0 12px; }
        .day-card p { margin-bottom: 8px; color: #536579; }
        .source {
            margin: 22px 0 0;
            color: #718094;
            font-size: 13px;
        }
        .source a { color: inherit; }
        @media (max-width: 720px) {
            .current, .forecast-grid { grid-template-columns: 1fr; }
            .topbar { align-items: flex-start; flex-direction: column; }
            form { flex-direction: column; }
        }
    </style>
</head>
<body>
<main>
    <section class="panel">
        <div class="topbar">
            <div>
                <h1>城市天气预报</h1>
                <p class="subtitle">输入中文或英文城市名，查询当前天气和未来三天预报。</p>
            </div>
            <a class="back" href="index.jsp">返回当前时间</a>
        </div>

        <form method="get" action="weather.jsp">
            <input name="city" value="<%= html(submittedCity) %>"
                   maxlength="80" placeholder="例如：北京、上海、Hangzhou" required>
            <button type="submit">查询天气</button>
        </form>

        <%-- 页面有三种状态：查询失败、尚未查询、查询成功。 --%>
        <% if (errorMessage != null) { %>
            <div class="error"><%= html(errorMessage) %></div>
        <% } else if (weather == null) { %>
            <div class="notice">
                查询会由 JSP 服务器端访问公开天气接口。网络不可用时会显示错误提示，
                原有的当前时间页面仍可独立使用。
            </div>
        <% } else { %>
            <h2><%= html(weather.location().displayName()) %></h2>
            <p class="subtitle">
                数据时间：<%= html(weather.observationTime()) %>，
                时区：<%= html(weather.location().timezone()) %>
            </p>

            <div class="current">
                <div class="metric">
                    <div class="metric-label">当前天气</div>
                    <div class="metric-value temperature">
                        <%= String.format("%.1f", weather.temperature()) %> °C
                    </div>
                    <div><%= html(weather.description()) %></div>
                </div>
                <div class="metric">
                    <div class="metric-label">体感温度</div>
                    <div class="metric-value">
                        <%= String.format("%.1f", weather.apparentTemperature()) %> °C
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">相对湿度</div>
                    <div class="metric-value"><%= weather.relativeHumidity() %>%</div>
                </div>
                <div class="metric">
                    <div class="metric-label">风速</div>
                    <div class="metric-value">
                        <%= String.format("%.1f", weather.windSpeed()) %> km/h
                    </div>
                </div>
            </div>

            <h2>未来三天</h2>
            <%-- dailyForecasts 中每个元素对应一天，循环生成三张预报卡片。 --%>
            <div class="forecast-grid">
                <% for (DailyForecast day : weather.dailyForecasts()) { %>
                    <article class="day-card">
                        <h3><%= html(day.date()) %></h3>
                        <p><strong><%= html(day.description()) %></strong></p>
                        <p>
                            最高 <%= String.format("%.1f", day.maximumTemperature()) %> °C /
                            最低 <%= String.format("%.1f", day.minimumTemperature()) %> °C
                        </p>
                        <p>
                            最大降水概率：
                            <%= day.precipitationProbability() == null
                                    ? "暂无" : day.precipitationProbability() + "%" %>
                        </p>
                    </article>
                <% } %>
            </div>
        <% } %>

        <p class="source">
            天气与地点数据由
            <a href="https://open-meteo.com/" target="_blank" rel="noopener">Open-Meteo</a>
            提供。本页面仅用于课程作业演示。
        </p>
    </section>
</main>
</body>
</html>
