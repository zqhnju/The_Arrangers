"use strict";

const DAYS = ["周一", "周二", "周三", "周四", "周五", "周六", "周日"];
const START_HOUR = 8;
const SLOT_MINUTES = 10;
const SLOTS_PER_HOUR = 60 / SLOT_MINUTES;
const SLOTS_PER_DAY = (24 - START_HOUR) * SLOTS_PER_HOUR;
const LATE_NIGHT_START = (23 - START_HOUR) * SLOTS_PER_HOUR;

const week = document.getElementById("week");
const notice = document.getElementById("notice");
const statusText = document.getElementById("data-status");
const demoButton = document.getElementById("show-demo");

function slotToTime(slot) {
    const minutes = START_HOUR * 60 + slot * SLOT_MINUTES;
    const hour = String(Math.floor(minutes / 60)).padStart(2, "0");
    const minute = String(minutes % 60).padStart(2, "0");
    return `${hour}:${minute}`;
}

function createColumn(title, className) {
    const column = document.createElement("div");
    column.className = `column ${className}`;
    const heading = document.createElement("h3");
    heading.className = "column-heading";
    heading.textContent = title;
    const slots = document.createElement("div");
    slots.className = "slots";
    column.append(heading, slots);
    week.append(column);
    return slots;
}

function renderGrid() {
    week.replaceChildren();
    const timeColumn = createColumn("时间", "time-column");
    for (let slot = 0; slot < SLOTS_PER_DAY; slot += SLOTS_PER_HOUR) {
        const label = document.createElement("span");
        label.className = "time-label";
        label.style.gridRow = String(slot + 1);
        label.textContent = slotToTime(slot);
        timeColumn.append(label);
    }

    return DAYS.map((day) => {
        const column = createColumn(day, "day");
        const lateArea = document.createElement("div");
        lateArea.className = "late";
        lateArea.setAttribute("aria-hidden", "true");
        lateArea.style.gridRow = `${LATE_NIGHT_START + 1} / ${SLOTS_PER_DAY + 1}`;
        column.append(lateArea);
        return column;
    });
}

function isInterval(interval) {
    return interval && Number.isInteger(interval.day) &&
        interval.day >= 0 && interval.day < DAYS.length &&
        Number.isInteger(interval.start) && Number.isInteger(interval.end) &&
        interval.start >= 0 && interval.start < interval.end && interval.end <= SLOTS_PER_DAY;
}

function renderEvent(columns, interval, name, colorClass) {
    const { day, start, end } = interval;
    const time = `${slotToTime(start)}–${slotToTime(end)}`;
    const event = document.createElement("div");
    event.className = `event ${colorClass}`;
    event.style.gridRow = `${start + 1} / ${end + 1}`;
    event.title = `${name} · ${DAYS[day]} ${time}`;
    event.setAttribute("aria-label", event.title);
    event.tabIndex = 0;

    const label = document.createElement("span");
    label.className = "event-name";
    label.textContent = name;
    event.append(label);
    if (end - start > 1) {
        const timeLabel = document.createElement("span");
        timeLabel.className = "event-time";
        timeLabel.textContent = time;
        event.append(timeLabel);
    }
    columns[day].append(event);
}

function renderSchedule(data) {
    const columns = renderGrid();
    let scheduledSlots = 0;
    let lateSlots = 0;
    let skipped = 0;

    for (const course of data.courses) {
        if (!isInterval(course) || typeof course.name !== "string") {
            skipped++;
            continue;
        }
        renderEvent(columns, course, course.name, "course");
    }

    for (const task of data.tasks) {
        if (!task || typeof task.name !== "string" || !Array.isArray(task.assigned) ||
            !Number.isInteger(task.importance) || task.importance < 1 || task.importance > 5) {
            skipped++;
            continue;
        }
        for (const interval of task.assigned) {
            if (!isInterval(interval)) {
                skipped++;
                continue;
            }
            renderEvent(columns, interval, task.name, `priority-${task.importance}`);
            scheduledSlots += interval.end - interval.start;
            lateSlots += Math.max(0, interval.end - Math.max(interval.start, LATE_NIGHT_START));
        }
    }

    const { init_cost: initialCost, final_cost: finalCost } = data;
    document.getElementById("initial-cost").textContent =
        Number.isFinite(initialCost) ? initialCost.toFixed(2) : "—";
    document.getElementById("final-cost").textContent =
        Number.isFinite(finalCost) ? finalCost.toFixed(2) : "—";
    document.getElementById("scheduled-minutes").textContent = `${scheduledSlots * SLOT_MINUTES} 分钟`;
    document.getElementById("late-slots").textContent = `${lateSlots} 槽`;
    return skipped;
}

const data = window.SCHEDULE_DATA;
if (data && Array.isArray(data.courses) && Array.isArray(data.tasks)) {
    const skipped = renderSchedule(data);
    notice.hidden = skipped === 0;
    statusText.textContent = `有 ${skipped} 条数据格式不正确，已跳过。`;
} else {
    renderGrid();
    if (data !== undefined) {
        statusText.textContent = "排程文件缺少有效的 courses 或 tasks 数组。请检查导出结果。";
    }
    demoButton.hidden = false;
    demoButton.addEventListener("click", () => {
        renderSchedule(window.SCHEDULE_DEMO_DATA);
        statusText.textContent = "当前显示示例排程。生成 viewer_data.js 后，刷新可查看实际结果。";
        statusText.tabIndex = -1;
        statusText.focus();
        demoButton.hidden = true;
    });
}
