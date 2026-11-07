(function(root){'use strict';
const rows={
words:[
[
['The shop is closed on Sunday.','closed 在这里指什么？',['营业中','不营业','搬家了'],1,'closed 描述商店不营业。'],
['Please bring a pen to the class.','bring 在这里是什么意思？',['带来','借走','丢弃'],0,'带着笔来上课。'],
['The train is late today.','late 表示什么？',['很贵','提前','晚点'],2,'late 描述晚于原定时间。'],
['Click the green button to start.','start 表示什么？',['开始','删除','停下'],0,'点击按钮开始。'],
['The cable is under the desk.','under 表示什么位置？',['上面','下面','旁边'],1,'under 表示在下面。'],
['This file is new.','new 在这里是什么意思？',['损坏的','旧的','新的'],2,'new 指新的文件。']
],[
['Could you give me a receipt for the taxi?','receipt 在这里指什么？',['路线','收据','预约'],1,'for the taxi 的付款情境指收据。'],
['The hotel can arrange an early breakfast.','arrange 在这里表示什么？',['取消','忘记','安排'],2,'酒店可以安排提早用早餐。'],
['Please keep the door clear.','keep ... clear 在这里要求什么？',['别挡住门','把门上锁','给门涂漆'],0,'门口要保持畅通。'],
['Save a copy before you edit the document.','edit 表示什么？',['打印','编辑','购买'],1,'修改文档前先保存副本。'],
['The battery lasts for eight hours.','lasts 在这里指什么？',['能持续使用','排在最后','正在充电'],0,'last 在时长语境中表示持续。'],
['The link takes you to the support page.','support 在这里指什么？',['广告','运输','帮助与支持'],2,'产品网页的 support 指帮助服务。']
],[
['We need a flexible booking because the dates may change.','flexible 在这里表示什么？',['必须提前付款','可调整的','完全免费的'],1,'日期可能变化，因此需要可调整的预订。'],
['The new rule applies to visitors as well as employees.','applies to 在这里是什么意思？',['由……提出','拒绝……','适用于……'],2,'规则也适用于访客。'],
['We reached a compromise on the delivery date.','compromise 在这里指什么？',['双方让步后的约定','单方面取消','严格保密'],0,'双方各有让步达成一致。'],
['The patch addresses a security weakness.','addresses 在这里指什么？',['邮寄','处理并解决','公布地址'],1,'patch 处理安全弱点。'],
['The service is reliable even during busy periods.','reliable 表示什么？',['值得信赖、稳定的','总会失败的','尚未开放的'],0,'繁忙期间也稳定。'],
['The test reproduces the fault consistently.','reproduces 在这里指什么？',['隐藏','预防','重现'],2,'测试能稳定重现故障。']
],[
['The estimate is tentative, pending confirmation from the supplier.','tentative 在这里表示什么？',['已最终批准的','暂定的','故意虚假的'],1,'仍等供应商确认，所以是暂定估计。'],
['The committee endorsed the proposal with reservations.','with reservations 在这里表示什么？',['提前订座','完全否定','仍有保留意见'],2,'支持提案但没有完全认可所有方面。'],
['The revised policy inadvertently excluded part-time staff.','inadvertently 在这里表示什么？',['无意中','反复地','按要求'],0,'修订产生了非本意的排除。'],
['The interface abstracts away the implementation details.','abstracts away 在这里指什么？',['复制全部细节','隐藏底层细节','禁止所有调用'],1,'接口让使用者不必处理实现细节。'],
['The improvement is marginal relative to the added complexity.','marginal 在这里表示什么？',['幅度很小的','无法测量的','已经失败的'],0,'相对于复杂度成本，改善很小。'],
['The apparent correlation does not substantiate a causal claim.','substantiate 在这里是什么意思？',['反对所有研究','简化表达','提供充分依据'],2,'相关并不足以为因果主张提供充分依据。']
]],
read:[
[
['Café opening hours: Monday to Friday, 8 am to 5 pm. Closed at weekends.','周日可以去喝咖啡吗？',['可以，八点开门','不可以','下午五点以后可以'],1,'周末不营业。'],
['My name is Ali. I live in Leeds. My sister lives in York.','Ali 住在哪里？',['York','Leeds','文中没有说'],1,'第一人称的 I 指 Ali。'],
['Your table is ready. Please come to the front desk.','客人现在应该做什么？',['回家等电话','取消订位','去前台'],2,'come to the front desk 是当前行动。'],
['Printer: ON. Paper tray: empty. Add paper before printing.','打印前先要做什么？',['加纸','关机','换密码'],0,'纸盒为空，需要加纸。'],
['Do not remove the cable while the light is flashing.','灯闪烁时应该怎样？',['拔下线缆','保持连接','重新开灯'],1,'Do not remove 表示不要拔线。'],
['The photo is saved in the Pictures folder, not Downloads.','照片保存在哪？',['桌面','Downloads','Pictures'],2,'not 排除了 Downloads。']
],[
['Please collect your parcel after 2 pm. Bring photo ID. The desk closes at 6 pm.','下午一点带着证件能取件吗？',['能，证件就够了','不能，要两点以后','不能，全天都关门'],1,'必须同时满足时间与证件要求。'],
['Rita works from home on Tuesday. On Thursday she is in the office until lunch, then visits a client.','周四下午 Rita 在哪里？',['在家','见客户','一直在办公室'],1,'then 表明午餐后去见客户。'],
['The museum ticket includes the garden. The special exhibition needs a separate ticket.','普通票不包含什么？',['花园','博物馆','特别展览'],2,'特别展览另购票。'],
['A verification message has been sent. Open it and confirm your address within one hour.','下一步应该做什么？',['打开邮件并确认','等待一整天','重新创建账号'],0,'信息已发送，需要在一小时内确认。'],
['The app works offline after the map has been downloaded. You need a connection for the first download.','什么时候不需要联网？',['首次下载时','地图下载完成后使用时','任何时候都不能离线'],1,'首次下载与后续使用分开。'],
['Only the owner can delete this folder. Other members can view and add files.','普通成员可以删除文件夹吗？',['可以，只要先查看','可以，添加文件后即可','不可以'],2,'Only the owner 限定删除权限。']
],[
['The train ticket is cheaper, but it cannot be changed. Since our appointment may move, we chose the flexible fare despite its higher price.','他们为什么选更贵的票？',['需要更改时间的余地','便宜票已经售罄','预约已经取消'],0,'may move 与 flexible 构成选择原因。'],
['The library has extended its opening hours for the exam period. However, the help desk still closes at five, so borrow specialist equipment before then.','晚上仍可进图书馆，但要注意什么？',['所有书都不能借','设备借用受服务台时间限制','考试期间图书馆不开门'],1,'开放时间延长不代表服务台也延长。'],
['The supplier offered a replacement rather than a refund. We accepted because the revised model meets our requirements and can arrive before the event.','接受方案的主要依据是什么？',['退款比较慢且文中已确认','不再需要产品','替代品符合需求且能及时到货'],2,'需求与交期都是接受理由，退款速度未提及。'],
['The server is reachable, yet logins fail for new accounts only. Existing users can still sign in. The team is checking account creation rather than the network.','哪种解释更符合现象？',['所有网络都断开了','新账号创建流程可能出问题','现有用户全部被禁止登录'],1,'仅新账号受影响，且服务器可达。'],
['The upgrade improves performance but removes an older file format. Teams depending on that format should convert their archives before upgrading.','依赖旧格式的团队应先做什么？',['转换存档','立即删除全部存档','永远不能升级'],0,'before upgrading 给出升级前的准备。'],
['We tested the backup by restoring a sample, not merely checking that a file existed. The restored records matched the source.','测试提供了什么额外证据？',['所有未来故障都不会发生','备份没有占空间','样本确实能恢复且内容一致'],2,'恢复样本比只检查文件存在提供更多依据，但不保证未来。']
],[
['Although the survey suggests broad support, respondents volunteered and were mostly existing members. The findings therefore inform service improvements but should not be treated as representative of the whole city.','作者如何限定结论？',['可指导改进，但样本不足以代表全市','证明全市居民一致支持','因为有人自愿就完全无效'],0,'承认用途，同时限定代表性。'],
['The contract allows cancellation without a fee unless production has begun. The supplier has ordered materials but has not started manufacturing. Ordering materials alone does not count as production under this clause.','按这项条款，现在取消要收费吗？',['一定收费，因为已订材料','不用，尚未开始生产','无法判断，因为条款未定义生产'],1,'条款明确区分订材料和开始生产。'],
['The proposal trades shorter delivery times for a narrower range of options. Customers who value customisation may prefer the existing service, even if they approve of faster delivery in principle.','作者认为支持快递送一定意味着支持新方案吗？',['是，因为选项完全一样','是，所有客户只在意速度','不一定，定制选择也影响偏好'],2,'in principle 的支持不等于接受整个权衡。'],
['A lower average response time can conceal slower responses at the upper end of the distribution. The release note reports both the median and the ninety-fifth percentile, because a single average would not capture the experience of users facing long delays.','为什么同时报告两个统计量？',['为了掩盖延迟','为了观察典型体验及长延迟尾部','因为均值从来不能计算'],1,'要避免单一平均数掩盖尾部体验。'],
['The experiment isolates the storage change while keeping the processor and workload fixed. Its results support a claim about this configuration; they do not establish that the same gain will hold for larger workloads or a different processor.','哪项说法超出了证据？',['所有处理器与工作负载都会有同样增益','该配置下测到了变化','比较时控制了处理器'],0,'作者明确限制外推。'],
['Encrypting traffic mitigates interception but does not prevent a compromised endpoint from exposing plaintext. Treating encryption as a complete solution would conflate protection in transit with protection at either end.','作者指出什么概念区别？',['明文永远不能出现','加密没有任何用途','传输途中保护与端点保护不同'],2,'加密的保护边界不覆盖已受损端点。']
]],
listen:[
[
['A: Would you like tea or coffee? B: Tea, please. No sugar.','客人要什么？',['咖啡加糖','茶，不加糖','茶和咖啡'],1,'Tea, please. No sugar.'],
['A: What time is lunch? B: At twelve. A: Thank you.','午餐几点？',['十点','十一点','十二点'],2,'At twelve.'],
['A: Is this your bag? B: No, my bag is blue. This one is red.','说话者自己的包是什么颜色？',['蓝色','红色','没有说'],0,'my bag is blue。'],
['A: Is the screen on? B: Yes, but there is no picture.','屏幕是什么情况？',['关着','开着但没有画面','正在显示照片'],1,'on 与 no picture 同时成立。'],
['A: Which button? B: The small one on the left.','应该按哪个按钮？',['左边小的','右边大的','中间的'],0,'small one on the left。'],
['A: Where is the cable? B: In the box next to the computer.','线缆在哪里？',['电脑下面','桌子外面','电脑旁的盒子里'],2,'in the box next to the computer。']
],[
['A: Can we meet at ten? B: I have a call then. How about half past ten? A: That works.','最终约几点？',['十点','十点半','九点半'],1,'接受了 half past ten 的建议。'],
['A: A room for two nights? B: Actually, three. I will leave on Friday. A: I will change the booking.','客人最终住几晚？',['两晚','四晚','三晚'],2,'Actually three 更正了 two。'],
['A: Can I pay by card? B: Usually yes, but the machine is not working today. Cash only, I am afraid.','今天可以怎样付款？',['现金','只能银行卡','不能付款'],0,'通常可以刷卡，但今天只能现金。'],
['A: Do I restart now? B: Save your work first. Then close the app and restart the computer.','重启前首先做什么？',['删除应用','保存工作','拔掉电源'],1,'first 指保存工作。'],
['A: The message says the password is wrong. B: Check the capital letters before you reset it.','重置密码前建议做什么？',['检查大小写','新建账号','卸载软件'],0,'check the capital letters before reset。'],
['A: Did you send the report? B: I saved it, but I have not uploaded it yet. I will do that after lunch.','报告现在是什么状态？',['已上传','还没保存','已保存，未上传'],2,'saved 与 not uploaded 区分状态。']
],[
['A: Shall we book the cheaper flight? B: It lands after the last train. A: A taxi would cost more than the difference. B: Then let us take the earlier flight.','为什么选早班飞机？',['便宜的航班已取消','后续交通会抵消票价节省','他们不喜欢火车'],1,'taxi 的成本高于票价差。'],
['A: Can you cover my shift on Monday? B: Only if I can leave by four. A: That is fine; I will be back by then.','同意代班的条件是什么？',['必须整天加班','周一不工作','四点前能离开'],2,'Only if 明确条件。'],
['A: The venue is available on both dates. B: The speakers can only come on the later one. A: Let us confirm that date before paying the deposit.','决定日期主要受什么限制？',['演讲者的时间','场地两天都不可用','定金已付不可改'],0,'场地都可用，讲者只有后一天可用。'],
['A: The download has finished. Shall I install it? B: Not until we copy the current settings. A: Even with a backup? B: Yes, keep a separate copy for this change.','安装前还需要做什么？',['重新下载','另存当前设置','删除备份'],1,'not until 与 separate copy 给出准备条件。'],
['A: The test failed only when two clients connected at once. B: So a single-user test would miss it. A: Exactly. Let us add a concurrent test before release.','准备增加什么测试？',['多客户端同时连接','仅单人登录','只检查界面颜色'],0,'concurrent 表示并发，单用户测不到。'],
['A: Is the data gone? B: The files are still there; the index is damaged. A: Can we rebuild it? B: Yes, after we copy the files to a safe location.','当前建议的处理顺序是什么？',['先删除所有文件','立即重建，不用备份','先安全复制文件，再重建索引'],2,'after we copy 限定重建之前的步骤。']
],[
['A: The consultation attracted many comments. B: Yes, though we cannot infer how common each view is; people with strong opinions were more likely to respond. A: So we should use it to identify concerns, not estimate public support.','他们认为反馈最适合用来做什么？',['发现关切，而非估算全体支持比例','证明多数人反对','替代所有代表性调查'],0,'自愿回应的偏差限制比例推断。'],
['A: Could we promise delivery by Friday? B: Only provisionally. The component is due tomorrow, but customs clearance remains uncertain. A: Then let us offer an estimate and explain the dependency.','应该怎样对客户说明？',['保证周五一定到','给暂定时间并说明依赖条件','不说明任何时间或原因'],1,'交期仍取决于不确定的清关。'],
['A: The pilot saved time. B: For experienced staff, yes. New starters needed extra guidance. A: Then the overall figure hides an important difference. B: We should report the groups separately.','为什么分组报告？',['因为试点没有任何收益','为了删掉新员工数据','总体数值掩盖了不同人群的差异'],2,'熟练者与新员工结果不同。'],
['A: The new model uses less memory. B: But it repeats part of the computation for every chunk. A: So the smaller footprint does not guarantee lower latency. B: We need measurements on the target device.','他们反对哪一种推断？',['内存更小就一定更快','必须在目标设备测量','分块可能重复计算'],0,'小内存与低延迟并不必然一致。'],
['A: The encrypted channel passed inspection. B: That says little about the logs at the endpoint. A: Agreed. Credentials might still be written there. B: Let us audit logging before declaring the system secure.','接下来还要检查什么？',['网线颜色','端点日志是否暴露凭据','是否把加密全部关闭'],1,'传输加密不排除端点日志泄漏。'],
['A: Average throughput increased, but the slowest requests got worse. B: Then a blanket improvement claim would be misleading. A: We should state the trade-off and include tail latency.','报告应该怎样写？',['只写平均吞吐提高','宣布所有请求更快','说明权衡并包含尾部延迟'],2,'平均改善与最慢请求恶化需同时呈现。']
]]};
const items=[];for(const [skill,bands] of Object.entries(rows))bands.forEach((list,level)=>list.forEach((a,i)=>items.push({id:'ad-'+skill+'-'+level+'-'+i,skill,level,context:i<3?'daily':'tech',tag:skill==='words'?'语境词义':level<2?'事实与安排':'条件、推断与限制',text:a[0],question:a[1],options:a[2],answer:a[3],why:a[4]})));
const api={version:'adaptive-original-1',items,labels:['基础信息','日常沟通','独立理解','复杂信息'],sample:'Please listen to this short sentence. You can pause and continue later.'};if(typeof module==='object'&&module.exports)module.exports=api;else root.WORDCARD_ADAPTIVE_BANK=api;
})(typeof window==='object'?window:globalThis);
