# flake8: noqa
from yt_dashboard_generator.sensor import MultiSensor
from yt_dashboard_generator.backends.monitoring.sensors import MonitoringExpr
from yt_dashboard_generator.backends.monitoring import MonitoringTag

from ..common.sensors import *

##################################################################

def action_queue_utilization(sensor_cls):
    utilization_all = (
        (MonitoringExpr(sensor_cls("yt.action_queue.time.cumulative.rate")) /
            MonitoringExpr(sensor_cls("yt.resource_tracker.thread_count")))
        .all("thread")
        .alias("{{thread}} {{container}}")
        .top_max(10)
        .top(False))

    return utilization_all

cpu_usage = (lambda thread: MultiSensor(
    MonitoringExpr(TabNodeCpu("yt.resource_tracker.thread_count")).top_max(1).alias("Limit"),
    MonitoringExpr(TabNodeCpu("yt.resource_tracker.total_cpu")) / 100)
    .value("thread", thread))


def top_max_bottom_min(sensor):
    all_nodes = (MonitoringExpr(sensor)
        .value("container_category", "pod")
        .all(MonitoringTag("host"))
        .alias("Usage {{container}}")
    )
    return [all_nodes.top_max(5), all_nodes.bottom_min(5)]
