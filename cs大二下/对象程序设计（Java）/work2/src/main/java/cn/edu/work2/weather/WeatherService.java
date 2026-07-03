package cn.edu.work2.weather;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * 小作业二第 5 题选做：从 Open-Meteo 获取天气预报。
 *
 * 处理流程：
 * 1. 使用 Geocoding API 将用户输入的城市名转换为经纬度。
 * 2. 使用 Forecast API 查询当前天气和未来三天预报。
 * 3. 使用 Gson 解析 JSON，并转换为 JSP 易于展示的不可变数据对象。
 *
 * Open-Meteo 的普通非商业接口不要求 API Key，可直接用于天气查询。
 */
public final class WeatherService {
    // 第一个接口负责“城市名 -> 经纬度”，第二个接口负责“经纬度 -> 天气”。
    private static final String GEOCODING_ENDPOINT =
            "https://geocoding-api.open-meteo.com/v1/search";
    private static final String FORECAST_ENDPOINT =
            "https://api.open-meteo.com/v1/forecast";
    private static final Duration REQUEST_TIMEOUT = Duration.ofSeconds(8);

    private final HttpClient httpClient;

    public WeatherService() {
        // HttpClient 可复用连接；连接超时与单次请求超时分别设置。
        httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(5))
                .followRedirects(HttpClient.Redirect.NORMAL)
                .build();
    }

    /**
     * 按城市名称查询天气。
     *
     * @param city 用户输入的城市名或邮政编码
     * @return 当前天气和未来三天预报
     * @throws WeatherException 城市不存在、网络失败或响应格式异常
     */
    public WeatherReport getWeather(String city) throws WeatherException {
        String normalizedCity = city == null ? "" : city.trim();
        if (normalizedCity.length() < 2) {
            throw new WeatherException("请输入至少两个字符的城市名称。");
        }
        if (normalizedCity.length() > 80) {
            throw new WeatherException("城市名称过长，请重新输入。");
        }

        Location location = findLocation(normalizedCity);
        // 预报接口不直接接收城市名，因此必须先完成地理编码。
        return fetchForecast(location);
    }

    private Location findLocation(String city) throws WeatherException {
        // 城市名可能含中文或空格，拼接 URL 前必须进行 UTF-8 百分号编码。
        String encodedCity = URLEncoder.encode(city, StandardCharsets.UTF_8);
        URI uri = URI.create(GEOCODING_ENDPOINT
                + "?name=" + encodedCity
                + "&count=1&language=zh&format=json");

        JsonObject root = requestJson(uri);
        JsonArray results = root.getAsJsonArray("results");
        if (results == null || results.size() == 0) {
            throw new WeatherException("没有找到城市“" + city + "”，请尝试输入更完整的名称。");
        }

        // 当前实现使用匹配度最高的第一条地点结果。
        JsonObject item = results.get(0).getAsJsonObject();
        return new Location(
                requiredString(item, "name"),
                optionalString(item, "admin1"),
                optionalString(item, "country"),
                requiredDouble(item, "latitude"),
                requiredDouble(item, "longitude"),
                requiredString(item, "timezone")
        );
    }

    private WeatherReport fetchForecast(Location location) throws WeatherException {
        // Locale.ROOT 可避免部分系统区域设置把小数点格式化成逗号，导致 URL 无效。
        String query = String.format(Locale.ROOT,
                "?latitude=%.6f&longitude=%.6f"
                        + "&current=temperature_2m,relative_humidity_2m,"
                        + "apparent_temperature,weather_code,wind_speed_10m"
                        + "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
                        + "precipitation_probability_max"
                        + "&timezone=auto&forecast_days=3",
                location.latitude(), location.longitude());

        JsonObject root = requestJson(URI.create(FORECAST_ENDPOINT + query));
        JsonObject current = requiredObject(root, "current");
        JsonObject daily = requiredObject(root, "daily");

        List<DailyForecast> forecasts = parseDailyForecasts(daily);
        return new WeatherReport(
                location,
                requiredString(current, "time"),
                requiredDouble(current, "temperature_2m"),
                requiredDouble(current, "apparent_temperature"),
                requiredInt(current, "relative_humidity_2m"),
                requiredDouble(current, "wind_speed_10m"),
                weatherDescription(requiredInt(current, "weather_code")),
                forecasts
        );
    }

    private List<DailyForecast> parseDailyForecasts(JsonObject daily)
            throws WeatherException {
        JsonArray dates = requiredArray(daily, "time");
        JsonArray codes = requiredArray(daily, "weather_code");
        JsonArray maximums = requiredArray(daily, "temperature_2m_max");
        JsonArray minimums = requiredArray(daily, "temperature_2m_min");
        JsonArray precipitation = requiredArray(daily, "precipitation_probability_max");

        int count = dates.size();
        // 同一天的日期、天气代码和温度必须位于各数组的相同下标。
        if (codes.size() != count || maximums.size() != count
                || minimums.size() != count || precipitation.size() != count) {
            throw new WeatherException("天气服务返回的预报数组长度不一致。");
        }

        // 将接口的“多个平行数组”转换成 JSP 更易遍历的逐日对象列表。
        List<DailyForecast> forecasts = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            forecasts.add(new DailyForecast(
                    dates.get(i).getAsString(),
                    weatherDescription(codes.get(i).getAsInt()),
                    maximums.get(i).getAsDouble(),
                    minimums.get(i).getAsDouble(),
                    precipitation.get(i).isJsonNull()
                            ? null : precipitation.get(i).getAsInt()
            ));
        }
        return List.copyOf(forecasts);
    }

    private JsonObject requestJson(URI uri) throws WeatherException {
        // 单次请求设置超时，避免外部网站无响应时 JSP 页面永久等待。
        HttpRequest request = HttpRequest.newBuilder(uri)
                .timeout(REQUEST_TIMEOUT)
                .header("Accept", "application/json")
                .header("User-Agent", "Java-Course-Weather-Demo/1.0")
                .GET()
                .build();

        try {
            HttpResponse<String> response = httpClient.send(
                    request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
            if (response.statusCode() != 200) {
                throw new WeatherException(
                        "天气服务暂时不可用，HTTP 状态码：" + response.statusCode());
            }

            JsonObject root = JsonParser.parseString(response.body()).getAsJsonObject();
            // Open-Meteo 有时会用 HTTP 200 携带 error 字段，因此还要检查响应内容。
            if (root.has("error") && root.get("error").getAsBoolean()) {
                String reason = root.has("reason")
                        ? root.get("reason").getAsString() : "未知原因";
                throw new WeatherException("天气服务返回错误：" + reason);
            }
            return root;
        } catch (InterruptedException e) {
            // 捕获中断后恢复标志，避免上层丢失线程已经被要求停止的信息。
            Thread.currentThread().interrupt();
            throw new WeatherException("天气查询被中断，请稍后重试。", e);
        } catch (IOException | RuntimeException e) {
            throw new WeatherException("无法连接或解析天气服务，请检查网络后重试。", e);
        }
    }

    private static JsonObject requiredObject(JsonObject object, String name)
            throws WeatherException {
        // 集中校验字段，避免页面最终因空值或类型错误产生难懂的 HTTP 500。
        if (!object.has(name) || !object.get(name).isJsonObject()) {
            throw new WeatherException("天气数据缺少字段：" + name);
        }
        return object.getAsJsonObject(name);
    }

    private static JsonArray requiredArray(JsonObject object, String name)
            throws WeatherException {
        if (!object.has(name) || !object.get(name).isJsonArray()) {
            throw new WeatherException("天气数据缺少字段：" + name);
        }
        return object.getAsJsonArray(name);
    }

    private static String requiredString(JsonObject object, String name)
            throws WeatherException {
        if (!object.has(name) || object.get(name).isJsonNull()) {
            throw new WeatherException("天气数据缺少字段：" + name);
        }
        return object.get(name).getAsString();
    }

    private static String optionalString(JsonObject object, String name) {
        return object.has(name) && !object.get(name).isJsonNull()
                ? object.get(name).getAsString() : "";
    }

    private static double requiredDouble(JsonObject object, String name)
            throws WeatherException {
        if (!object.has(name) || object.get(name).isJsonNull()) {
            throw new WeatherException("天气数据缺少字段：" + name);
        }
        return object.get(name).getAsDouble();
    }

    private static int requiredInt(JsonObject object, String name)
            throws WeatherException {
        if (!object.has(name) || object.get(name).isJsonNull()) {
            throw new WeatherException("天气数据缺少字段：" + name);
        }
        return object.get(name).getAsInt();
    }

    /**
     * 将 Open-Meteo 使用的 WMO 天气代码转换为中文描述。
     */
    public static String weatherDescription(int code) {
        return switch (code) {
            case 0 -> "晴朗";
            case 1 -> "大致晴朗";
            case 2 -> "局部多云";
            case 3 -> "阴天";
            case 45, 48 -> "有雾";
            case 51, 53, 55 -> "毛毛雨";
            case 56, 57 -> "冻毛毛雨";
            case 61, 63, 65 -> "降雨";
            case 66, 67 -> "冻雨";
            case 71, 73, 75, 77 -> "降雪";
            case 80, 81, 82 -> "阵雨";
            case 85, 86 -> "阵雪";
            case 95 -> "雷暴";
            case 96, 99 -> "雷暴伴冰雹";
            default -> "未知天气";
        };
    }

    public record Location(
            String name,
            String admin1,
            String country,
            double latitude,
            double longitude,
            String timezone) {

        /**
         * 组合城市、省/州和国家名称，避免空字段产生多余分隔符。
         */
        public String displayName() {
            List<String> parts = new ArrayList<>();
            parts.add(name);
            if (!admin1.isBlank() && !admin1.equals(name)) {
                parts.add(admin1);
            }
            if (!country.isBlank()) {
                parts.add(country);
            }
            return String.join("，", parts);
        }
    }

    public record DailyForecast(
            String date,
            String description,
            double maximumTemperature,
            double minimumTemperature,
            Integer precipitationProbability) {
    }

    /**
     * JSP 使用的汇总结果。record 自动生成构造器和只读访问方法，
     * 适合作为服务层向页面传递数据的简单对象。
     */
    public record WeatherReport(
            Location location,
            String observationTime,
            double temperature,
            double apparentTemperature,
            int relativeHumidity,
            double windSpeed,
            String description,
            List<DailyForecast> dailyForecasts) {
    }

    public static class WeatherException extends Exception {
        public WeatherException(String message) {
            super(message);
        }

        public WeatherException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
