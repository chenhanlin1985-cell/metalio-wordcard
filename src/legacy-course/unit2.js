(function(root){
'use strict';
// Original adult listening/reading lessons aligned to the published Unit 2 objectives.
// American IPA; words and phrases count as target-expression positions, not publisher totals.
function wordRows(id,rows,extra=false){return rows.map((r,i)=>({id:`${id}-${extra?'x':'w'}${i+1}`,word:r[0],ipa:r[1],meaning:r[2],example:r[3],lesson:id}));}
const lessons=[];
function add(spec,rows,extras=[]){lessons.push({...spec,unit:2,revision:1,role:'lesson',words:wordRows(spec.id,rows),extraWords:wordRows(spec.id,extras,true)});}
add({id:'u2a',code:'2A',title:'读懂职业与工作介绍',time:'20–28 分钟',goal:'区分职业、工作地点和工作时间，听懂某人做什么、不做什么；把第一单元的人物介绍延伸到工作。',focus:'一般现在时肯定与否定；第三人称 works / does not work；职业与工作场所',recycle:['u1d-w2','u1a-w6','u1b-w1','u1d-w6'],
reading:'Mei works at a local library in Bristol. She is a librarian. Her colleague Ben is friendly and helpful. He is an engineer at a small software company. He works with Mei on the library’s software. He works in an office, not in a hospital. Ben works full-time. His cousin Leo is a manager at a shop. Leo works part-time because he also studies. Their neighbour Rosa works in a café. Her friend Sam is a waiter there. Maya is a nurse at the local hospital; her brother is a doctor. Eva is a teacher and teaches adults. Arun is a driver. He takes people to work. At the English centre, a receptionist answers questions about classes. A job title tells you what someone does, but it does not always tell you where or how many hours they work.',
readQuestions:[
 ['Ben 的职业和工作地点分别是什么？',['工程师，在办公室','护士，在医院','经理，在咖啡店'],0,'engineer 是职业，office 是地点；不要把职业和场所混为一谈。'],
 ['谁是兼职工作？',['Mei','Ben','Leo'],2,'Leo works part-time；Ben works full-time。文章未交代 Mei 的工时。'],
 ['Maya 和她哥哥是否做完全相同的工作？',['是，两个人都是医生','不是，她是护士，哥哥是医生','文章没有给出职业'],1,'两人在医疗领域工作，但 nurse 和 doctor 不是同一个职业。'],
 ['关于 Mei，文章可以确认什么？',['她住在图书馆','她在当地的图书馆工作','她每天只工作两小时'],1,'work at 是工作地点；没有给出住址或具体工作时数。复用第一单元的 local 和工作信息。']
 ],dialogues:[
 {text:'A: What does Ben do? B: He is an engineer. A: Does he work in a hospital? B: No, he works in an office at a software company.',q:'Ben 在哪里工作？',options:['医院','办公室','餐厅'],answer:1,why:'先识别 engineer，再听地点的否定与更正：not a hospital, an office。'},
 {text:'A: Is Leo a driver? B: No, he is a manager in a shop. A: Does he work full-time? B: No, he works part-time. He studies too.',q:'哪一项符合 Leo 的情况？',options:['兼职经理，也学习','全职司机，不学习','全职医生，也学习'],answer:0,why:'要同时保留职业 manager 和 part-time 两条信息；driver 是被否定的猜测。'},
 {text:'A: I work at the local hospital. B: Are you a doctor, Maya? A: No, I am a nurse. My brother is a doctor.',q:'Maya 的职业是什么？',options:['接待员','医生','护士'],answer:2,why:'doctor 属于她哥哥，不能把两个人的信息交换。'}
 ],gap:['Ben ___ at a company. He does not work in a hospital.',['work','works','working'],1,'Ben 是第三人称单数，陈述句用 works；does not 后面用原形 work。'],transfer:['你只知道一位新同事在医院工作，能否直接判断他是医生？',['能，医院工作都叫 doctor','不能，还需要知道他的具体职业','能，他一定是 manager'],1,'工作地点并不唯一决定职业；这也复用了第一单元“不凭单条信息猜测”的阅读方法。']},[
 ['work','/wɝːk/','工作（动词）','Ben works at a small company.'],
 ['teacher','/ˈtiːtʃɚ/','教师','Eva is a teacher at the English centre.'],
 ['nurse','/nɝːs/','护士','Maya is a nurse at the local hospital.'],
 ['doctor','/ˈdɑːktɚ/','医生','Her brother is a doctor.'],
 ['engineer','/ˌendʒɪˈnɪr/','工程师','Ben is an engineer.'],
 ['receptionist','/rɪˈsepʃənɪst/','接待员','The receptionist helps new students.'],
 ['driver','/ˈdraɪvɚ/','司机','Arun is a driver in Bristol.'],
 ['waiter','/ˈweɪtɚ/','服务员（通常指男服务员）','Sam is a waiter at the café.'],
 ['manager','/ˈmænɪdʒɚ/','经理；管理者','Leo is a manager at a shop.'],
 ['office','/ˈɑːfɪs/','办公室','Ben works in an office.'],
 ['hospital','/ˈhɑːspɪtəl/','医院','Maya works in a hospital.'],
 ['company','/ˈkʌmpəni/','公司','The company has a small office.'],
 ['full-time','/ˌfʊl ˈtaɪm/','全职的；全职地','Ben works full-time.'],
 ['part-time','/ˌpɑːrt ˈtaɪm/','兼职的；兼职地','Leo works part-time and studies.']
 ],[['librarian','/laɪˈbreriən/','图书管理员','Mei is a librarian.'],['software','/ˈsɔːftwer/','软件','Ben works at a software company.']]);
add({id:'u2b',code:'2B',title:'听懂学习习惯与时间',time:'18–25 分钟',goal:'从学习介绍和问答中找出学什么、怎样学、何时学，区分工作安排与学习安排。',focus:'一般现在时疑问和简短回答；do you / does he；时间搭配',recycle:['u2a-w1','u1b-w8','u1d-w4','u1c-w4'],
reading:'Mei and Ben study English together on Tuesday evenings. Their class starts at seven. The teacher gives them a short listening lesson and some homework. Mei works during the day, so she studies at night. She listens to English in the morning on her way to work and reads her notes in the afternoon. Her favourite subject is language. Ben works full-time. He does not study late every night. He does ten minutes of listening practice after work. At the weekend, he takes an online course about computers. Sam works on Tuesday evenings, so he is in a different English class on Saturday mornings. He gets his homework by email. The three friends have different study times, but they all want to understand people more easily.',
readQuestions:[
 ['Mei 和 Ben 的英语课是什么时候？',['周二上午七点','周二晚上七点','周六上午九点'],1,'Tuesday evenings 与 starts at seven 合起来表示周二晚七点；不是另一位人物的课。'],
 ['Ben 每晚都学习到很晚吗？',['是，他每晚都学到很晚','没有提到','不是，文中明确否定'],2,'does not study late every night 是否定，不应只抓住 late 和 every night。'],
 ['为什么 Sam 参加另一班英语课？',['他周二晚上要工作','他不想学英语','他在另一个国家'],0,'because 的意思由 so 前后关系表达：工作安排决定他选择周六班。'],
 ['关于 Ben 的线上课程，哪一项有依据？',['是每天晚上的英语课','周末学习计算机相关内容','只能在医院学习'],1,'online course about computers 在 at the weekend；与周二英语 class 区别开。']
 ],dialogues:[
 {text:'A: Do you study English in the morning, Mei? B: I listen to it in the morning, but my class is at night. A: On Tuesday? B: Yes, at seven.',q:'Mei 的正式英语课在什么时候？',options:['周二晚七点','每天早上七点','周六下午'],answer:0,why:'早上听英语与正式上课不是同一件事；后半段给出课的时间。'},
 {text:'A: Does Ben do his homework every night? B: No, he does not. He does it on Wednesday. A: Does he study online too? B: Yes, at the weekend.',q:'哪一项符合 Ben 的学习安排？',options:['每晚做作业','周末不上网学习','周三做作业，周末线上学习'],answer:2,why:'分别追踪 homework 与 online study 的时间，注意第一句的否定。'},
 {text:'A: Is Sam in your Tuesday class? B: No. He works on Tuesday evenings. His English class is on Saturday morning. A: Do you study the same language? B: Yes, English.',q:'Sam 为什么不参加周二班？',options:['他不学英语','他那个时段要工作','他已经结束所有课程'],answer:1,why:'work 是本课复现的前课词，解释了为什么选另一个时间的班。'}
 ],gap:['___ Ben study online at the weekend? Yes, he does.',['Do','Is','Does'],2,'一般现在时第三人称疑问用 Does＋主语＋动词原形 study；you / they 用 Do。'],transfer:['同事说“I work at night, but I study in the afternoon.”，应怎样理解？',['晚上学习，下午工作','晚上工作，下午学习','工作和学习都在早上'],1,'分别找到 work 和 study 后面的时间，不把两个动作的安排混在一起。']},[
 ['study','/ˈstʌdi/','学习；攻读（动词）','Mei studies English after work.'],
 ['course','/kɔːrs/','一门课程；一系列课程','Ben takes an online course about computers.'],
 ['class','/klæs/','班级；一次课','Our English class starts at seven.'],
 ['lesson','/ˈlesən/','一节课；一课的教学内容','This listening lesson is short.'],
 ['practice','/ˈpræktɪs/','练习（本课作名词）','Ben does listening practice after work.'],
 ['homework','/ˈhoʊmwɝːk/','家庭作业；课后作业','The teacher sends homework by email.'],
 ['subject','/ˈsʌbdʒekt/','学科；科目','What is your favourite subject?'],
 ['online','/ˌɑːnˈlaɪn/','在线的；在线地','Ben studies online at the weekend.'],
 ['at night','/æt naɪt/','在夜间；在晚上','Mei studies at night.'],
 ['in the morning','/ɪn ðə ˈmɔːrnɪŋ/','在上午；在早晨','I listen to English in the morning.'],
 ['in the afternoon','/ɪn ði ˌæftɚˈnuːn/','在下午','She reads her notes in the afternoon.'],
 ['at the weekend','/æt ðə ˈwiːkend/','在周末（英式常用搭配）','Ben takes an online course at the weekend.']
 ]);
add({id:'u2c',code:'2C',title:'礼貌索取物品并回应',time:'15–22 分钟',goal:'在课堂、接待处和咖啡店听懂索取、递交、补充请求与拒绝，区分请求和已经得到的物品。',focus:'Can I have ...?；礼貌请求和回应；复用 Could you ...?',recycle:['u1c-w9','u1c-w6','u2a-w6','u2b-w3'],
reading:'Before her English class, Mei goes to the reception desk. She says to the receptionist, “Can I have a pen, please?” The receptionist gives her one and says, “Here you are.” Mei says, “Thank you.” Later, Ben and Mei visit Rosa’s café. Ben asks for a coffee and Mei asks for water. Rosa asks, “Anything else?” They reply, “That’s all, thanks.” If you do not hear a request clearly, say, “Sorry, could you repeat that, please?” Sorry can introduce a polite request to hear something again; it does not always mean that you have done something wrong. A request is not the same as confirmation that an item is available.',
readQuestions:[
 ['Mei 在接待处要什么？',['咖啡','水','一支笔'],2,'先区分接待处与咖啡店两个场景；pen 是上课前索取的物品。'],
 ['“Here you are.” 在这里是什么意思？',['给你（递交物品）','你住在哪里','请重复一遍'],0,'这句话跟随递笔动作，不是询问位置。'],
 ['“That’s all, thanks.” 表明什么？',['需要更多咖啡','不再加要其他东西','没听清楚'],1,'对 Anything else? 的回应，表明请求已经结束。']
 ],dialogues:[
 {text:'A: Can I have a pen, please? B: Here you are. A: Thank you. B: Anything else? A: No, that is all, thanks.',q:'对方最后还需要其他物品吗？',options:['需要一杯水','没有其他需要','需要另一支笔'],answer:1,why:'听到 Anything else? 后还要继续听回应；that is all 表示没有其他需要。'},
 {text:'A: Can I have a coffee, please? B: Sorry, we do not have coffee now. We have water. A: Water is fine, thank you. B: Here you are.',q:'说话者最终拿到什么？',options:['水','咖啡','两种都有'],answer:0,why:'最初想要 coffee，但咖啡没有；最终接受并拿到 water。'},
 {text:'A: Can I have a pen, please? B: Sorry, could you repeat that? A: A pen, please. B: Of course. Here you are.',q:'为什么对方请求重复？',options:['要更换姓名','在拒绝任何帮助','第一次没听清索取的东西'],answer:2,why:'Could you repeat that? 承接第一单元的核对用语；不是拒绝。'},
 {text:'A: Can I have water, please? B: Yes. Anything else? A: Can I have a coffee for Ben too? B: Of course. A: Thank you. That is all.',q:'最终一共要哪些饮品？',options:['只有水','只有咖啡','水和给 Ben 的咖啡'],answer:2,why:'Anything else? 后新增了 for Ben 的咖啡，要综合整段请求。'}
 ],gap:['A: Can I have a pen, please? B: ___.',['Here you are','I am from Bristol','At the weekend'],0,'递交物品时用 Here you are；其他两项分别说明来源和时间。'],transfer:['同事说“Sorry, could you repeat that, please?”，下一步最合适的是？',['结束谈话，不再回应','把刚才的话再说一次','直接登记为拒绝'],1,'这里 sorry 引出礼貌的重复请求，不能机械理解为道歉或拒绝。']},[
 ['Can I have ...?','/kæn aɪ hæv/','可以给我……吗？（索取物品）','Can I have a pen, please?'],
 ['Here you are.','/hɪr juː ɑːr/','给你。（递交东西时）','Here you are. This pen is for you.'],
 ['Anything else?','/ˈeniθɪŋ els/','还需要别的吗？','Anything else? A coffee for Ben?'],
 ['That’s all, thanks.','/ðæts ɔːl θæŋks/','就这些，谢谢。','That’s all, thanks. We do not need anything else.'],
 ['sorry','/ˈsɑːri/','对不起；不好意思（本课用于礼貌请求）','Sorry, could you repeat that, please?'],
 ['please','/pliːz/','请（礼貌用语）','Can I have water, please?'],
 ['pen','/pen/','钢笔；笔','Can I have a pen for the form?'],
 ['water','/ˈwɑːtɚ/','水','Water is fine, thank you.'],
 ['coffee','/ˈkɑːfi/','咖啡','Can I have a coffee for Ben?']
 ]);
add({id:'u2d',code:'2D',title:'读懂课程报名表',time:'14–20 分钟',goal:'把姓名、职业、课程和联系方式匹配到表单字段，分清必填与选填，核对报名后的实际安排。',focus:'原书填表目标的听读改编；字段与说明；必填、选填和信息更正',recycle:['u1c-w1','u1c-w2','u1c-w4','u1c-w8','u2a-w5','u2b-w2','u2b-w3','u2c-w7'],
reading:'Bristol English Centre. Course registration form. Please use capital letters for your name. First name: BEN. Surname: CARTER. Job: engineer. Course: English after work. Class: Tuesday at 7 p.m. Required information: first name, surname, email and signature. Optional information: phone number and date of birth. Ben uses ben.carter@example.com as his email. He leaves the optional phone number box empty. The centre uses email to contact him about the course. Check your name and class before you sign. Ben asks for a pen and adds his signature. A second page asks for an address and postcode only if you want a letter by post. Ben chooses email, so he does not complete that page. Required means you must provide the information on this form; optional means you can choose whether to provide it.',
readQuestions:[
 ['这张报名表中哪一项是必填？',['出生日期','签名','电话号码'],1,'Required information 中有 signature；另外两项位于 Optional information。此规则只针对这张示例表。'],
 ['Ben 留空电话号码，会不会单凭这一点就缺少必填信息？',['会，电话在所有表格都必填','无法阅读任何说明','不会，这张表中电话是选填'],2,'根据本表明确的 optional 标签作判断，不推广成所有报名表的规则。'],
 ['学校会如何联系 Ben？',['发邮件','去医院找他','必须寄纸质信'],0,'Ben 选择 email；邮寄地址那一页有条件，他没有选择邮寄。'],
 ['本表中的 Ben 是谁，选了哪节课？',['护士，周六上午课','工程师，周二晚七点课','经理，每天上午七点课'],1,'把第一单元的姓名信息与 2A 职业、2B 课程时间一一对应。'],
 ['签名前发现班级时间不正确，最合理的做法是什么？',['核对并更正后再签名','照签，表格一定不会错','删除所有必填信息'],0,'Check your name and class before you sign 说明先核对，再确认。']
 ],dialogues:[
 {text:'A: Do I need to write my phone number? B: No, that is optional on this form. Your email is required. A: Fine. Please contact me by email.',q:'这张表格中哪一项必须提供？',options:['电话号码','出生日期','邮箱'],answer:2,why:'phone number 明确 optional，email 才是 required。'},
 {text:'A: Your class is on Thursday at seven, Ben. B: Sorry, that is not correct. My class is on Tuesday at seven in the evening. A: Tuesday at seven in the evening. Is that right? B: Yes, thank you.',q:'最终确认的上课时间是什么？',options:['周二晚七点','周四晚七点','周二早上七点'],answer:0,why:'开头 Thursday 被更正；最后明确确认 Tuesday at seven in the evening（周二晚七点）。'},
 {text:'A: Can I have a pen, please? I need to add my signature. B: Here you are. A: Do I need to add my date of birth too? B: No, that is optional.',q:'说话者为什么要笔？',options:['必须填写出生日期','补上必填的签名','把课程换成线上课'],answer:1,why:'需求是 signature；后问的 date of birth 为选填，不能替代签名。'}
 ],gap:['Your email is ___. You must write it on this form.',['optional','quiet','required'],2,'must write 对应 required（必填），不是 optional（选填）。'],transfer:['另一张表格把 phone number 标为 required，你应该怎样理解？',['沿用上一张表的规则，仍可不填','按这张表的说明，电话号码是必填','required 表示可以任意选择'],1,'字段是否必填由当前表单说明决定，不能把示例表的规则机械迁移到所有表格。']},[
 ['form','/fɔːrm/','表格；表单','Please check the name on this form.'],
 ['required','/rɪˈkwaɪrd/','必需的；必填的','Your email is required on this form.'],
 ['optional','/ˈɑːpʃənəl/','可选择的；选填的','Your phone number is optional.'],
 ['date of birth','/ˌdeɪt əv ˈbɝːθ/','出生日期','The date of birth box is optional on this form.'],
 ['phone number','/ˈfoʊn ˌnʌmbɚ/','电话号码','Please check your phone number.'],
 ['signature','/ˈsɪɡnətʃɚ/','签名','Ben adds his signature to the form.']
 ],[['capital letters','/ˌkæpɪtəl ˈletɚz/','大写字母','Please use capital letters.'],['postcode','/ˈpoʊstkoʊd/','邮政编码（英式词）','Add a postcode if you choose a letter by post.']]);
const reviewReading='New members at Bristol English Centre. Sara is from Canada and lives in Bristol. She is a nurse at a hospital. She works part-time and studies English on Monday evenings. Her colleague Omar is a doctor. He works full-time and takes an online course at the weekend. Their neighbour Dan is a driver. He is quiet but helpful. He listens to English in the morning and does his homework in the afternoon. Sara and Dan are in the same English class. Omar is in a different class. They all use email to contact the centre.';
const requestReading='A note for new students. Before class, ask the receptionist if you need a pen. Say, “Can I have a pen, please?” If you do not hear the reply, ask the person to repeat it. At the café you can ask for coffee or water. When the waiter asks, “Anything else?”, say, “That’s all, thanks” if you do not want more. Always listen to the final reply: a requested item may not be available.';
const formReading='Evening course form. First name: Sara. Surname: Ali. Job: nurse. Class: Monday at 6 p.m. Required: first name, surname, email, signature. Optional: phone number, date of birth. Contact: sara.ali@example.com. Sara chooses email and leaves the optional phone number empty. Please check all required information before you sign. This form does not ask for a home address.';
lessons.push({id:'u2r',unit:2,code:'2R',title:'工作与学习 · 综合听读复习',role:'unit-review',revision:1,time:'18–25 分钟',words:[],extraWords:[],recycle:[],noLanguageDrills:true,focus:'work 的动词语境；职业、时间、礼貌请求和报名信息；跨单元人物与核对信息',goal:'在新的工作介绍、学习安排、请求和报名表中整合第二单元目标，并复用第一单元的来源、人物关系和信息核对。',reading:reviewReading,
readQuestions:[
 ['Sara 来自哪里？',['加拿大','Bristol','文章未说明'],0,'复用第一单元 from 与 live 的区别：来源 Canada，现居 Bristol。'],
 ['Sara 的工作与学习安排是什么？',['全职医生，周末线上学习','兼职护士，周一晚上学英语','兼职司机，每早上课'],1,'把 job、part-time 和 class 时间对应到同一个人。'],
 ['Omar 是 Sara 的什么人？',['邻居','家人','同事'],2,'colleague 是第一单元已学的人物关系词；Dan 才是 neighbour。'],
 ['谁和 Sara 在同一个英语班？',['Omar','Dan','没有人'],1,'相同工作地点不代表同班，文中明确写 Sara and Dan are in the same English class。'],
 ['“He works full-time.” 中 works 表示什么？',['进行全职工作','去看作品','在填写全名'],0,'work 在这里是动词“工作”；full-time 描述工作时间安排。'],
 ['咖啡店里听到“Anything else?”，若不再需要东西，应怎样回应？',["I am Canadian.","That’s all, thanks.",'My surname is Ali.'],1,'根据请求是否结束来回应，而非只挑熟悉的个人介绍句。',requestReading],
 ['这张表格中 Sara 必须提供什么？',['家庭地址','电话号码','邮箱与签名'],2,'两者在 required 栏；电话选填，家庭地址根本没要求。',formReading],
 ['Sara 的班级是哪一节？',['周一晚六点','周一早六点','周二晚七点'],0,'本题使用新的表单，不能沿用 Ben 的 Tuesday at seven。',formReading]
 ],dialogues:[
 {text:'A: Are you a doctor, Sara? B: No, I am a nurse. Omar is a doctor. A: Does he work part-time? B: No. He works full-time. I work part-time.',q:'哪一项正确？',options:['Sara 是全职医生','Omar 是兼职护士','Sara 兼职，Omar 全职'],answer:2,why:'既要追踪两个人，也要保留职业和工时的更正。'},
 {text:'A: Do you study in the morning, Dan? B: I listen to English then. I do my homework in the afternoon. A: Is your class on Monday evening? B: Yes, with Sara.',q:'Dan 什么时候做作业？',options:['上午','下午','没有提到'],answer:1,why:'听英语、做作业和正式上课分别有自己的时间。'},
 {text:'A: Can I have a coffee, please? B: Sorry, we do not have coffee today. A: Water is fine. B: Here you are. Anything else? A: No, that is all, thanks.',q:'这段请求最终怎样结束？',options:['拿到水，没有其他需要','拿到咖啡和水','没听清，要求重复'],answer:0,why:'请求从 coffee 改成 water；最后 that is all 表明结束。'},
 {text:'A: Is your class on Tuesday at six, Sara? B: No, Monday at six in the evening. A: Monday at six. Is that correct? B: Yes. A: Please add your signature. Your phone number is optional.',q:'哪一项是最终确认的信息？',options:['周二晚六点，电话必填','周一晚六点，需要签名','周一早六点，不需要签名'],answer:1,why:'综合第一单元的核对与第二单元的课程、必填信息；使用新人物 Sara 的表格。'}
 ]});
const api={lessons};if(typeof module==='object'&&module.exports)module.exports=api;else root.WORDCARD_UNIT2=api;
})(typeof window==='object'?window:globalThis);
