(function(root){
const items=[
 ['pv1','words','语境词义','Could I borrow your charger for a minute?','borrow 在这里是什么意思？',['借用','购买','修理'],0,'borrow 表示暂时借用，并非买下来。'],
 ['pv2','words','日常搭配','The meeting room is available after lunch.','available 在这里表示什么？',['非常嘈杂','可以使用','永久关闭'],1,'available 在此表示房间可以使用，不是描述大小或噪声。'],
 ['pv3','words','一词多义','Please charge the phone before the call.','charge 在这里是什么意思？',['收取费用','提出指控','给手机充电'],2,'phone 与 before the call 限定这里是充电的意思。'],
 ['pv4','words','科技语境','The update fixed the issue, so the app opens now.','issue 在这句话中指什么？',['问题','杂志期号','发行新版本'],0,'fixed 和 app opens 指向软件问题得到修复。'],
 ['pr1','read','提取信息','The library closes at six today. The café stays open until eight.','今天图书馆几点关门？',['八点','没有提到','六点'],2,'分别识别 library 与 café 的时间。'],
 ['pr2','read','更改安排','I work on Friday, so can we meet on Saturday instead? Please let me know before Thursday.','说话者提议什么时候见面？',['周四','周六','周五'],1,'Friday 是工作时间，Thursday 是回复截止时间，Saturday 才是见面建议。'],
 ['pr3','read','规则与例外','You can use the guest network without an account. Staff accounts are needed only for internal tools.','访客使用 guest network 需要员工账号吗？',['不需要','必须使用','文中禁止访客联网'],0,'without 和 only 划分了访客网络与内部工具的规则。'],
 ['pr4','read','条件与原因','We delayed the release because one test failed. The change is ready, but it will go live only after a second review.','修改已经准备好，为什么还没上线？',['所有测试都已经通过','因为取消了所有评审','还有第二次评审要完成'],2,'ready 不代表已经上线；only after 表明仍有前置条件。'],
 ['pl1','listen','提取信息','A: Where is the bus stop? B: It is next to the station, not the bank. A: Thank you.','公交站在哪里？',['银行旁边','车站旁边','图书馆里面'],1,'station 是肯定信息，bank 被 not 排除。'],
 ['pl2','listen','时间更正','A: Is the appointment on Wednesday at three? B: No, Thursday at three in the afternoon. A: Thursday afternoon? B: Yes.','最终确认的预约是什么时候？',['周四下午三点','周三下午三点','周四上午三点'],0,'Wednesday 是先前猜测，Thursday afternoon 是最终确认。'],
 ['pl3','listen','临时处理','A: Can I use the printer now? B: It is offline today. Please email me the file for now. We can print it tomorrow.','今天应该先做什么？',['直接打印','删除文件','把文件发邮件给对方'],2,'先发邮件是临时处理；打印安排在明天。'],
 ['pl4','listen','状态与条件','A: Have you installed the update? B: I have downloaded it, but I have not installed it yet. I will wait until the meeting finishes.','更新目前处于什么状态？',['已经安装','已下载，尚未安装','完全没有下载'],1,'downloaded 与 installed 是不同状态，not yet 限定安装尚未发生。']
].map(r=>({id:r[0],skill:r[1],tag:r[2],text:r[3],question:r[4],options:r[5],answer:r[6],why:r[7]}));
const bank={version:'path-probe-1',items,sample:'This is a sound check. You do not need to answer a question. Please adjust the volume until you can hear this sentence clearly.',scope:'12 original receptive-skill probes, not calibrated CEFR or vocabulary-size measurement'};
if(typeof module==='object'&&module.exports)module.exports=bank;else root.WORDCARD_PATH_BANK=bank;
})(typeof window==='object'?window:globalThis);
