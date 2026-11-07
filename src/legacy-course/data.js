(function(root){
const old=typeof module==='object'&&module.exports?require('./legacy-data.js'):root.WORDCARD_LEGACY_DATA;
const plan=typeof module==='object'&&module.exports?require('./curriculum.js'):root.WORDCARD_PLAN;
const lessons=JSON.parse(JSON.stringify(old.lessons));
function extend(id,rows){const l=lessons.find(x=>x.id===id);rows.forEach((w,i)=>l.words.push({id:`${id}-w${l.words.length+1}`,word:w[0],ipa:w[1],meaning:w[2],example:w[3],lesson:id}));}
extend('u1a',[
 ['British','/ˈbrɪtɪʃ/','英国的；英国人','My new neighbour is British.'],['French','/frentʃ/','法国的；法国人','She is French, but she lives in Bristol.'],['German','/ˈdʒɜːrmən/','德国的；德国人','Is your colleague German?'],['nationality','/ˌnæʃəˈnæləti/','国籍','Please check your nationality on the form.']
]);
extend('u1b',[
 ['married','/ˈmærid/','已婚的','My cousin is married.'],['single','/ˈsɪŋɡəl/','单身的','He is single and lives with a friend.']
]);
extend('u1c',[
 ['Could you ...?','/kʊd juː/','能请您……吗？','Could you repeat the number, please?'],['Is that ...?','/ɪz ðæt/','是……对吗？（核对信息）','Is that eighteen King Street?'],["That’s right.",'/ðæts raɪt/','是的，没错。',"Yes, that’s right. My surname is Park."]
]);
const a=lessons.find(x=>x.id==='u1a');Object.assign(a,{time:'18–25 分钟',recycle:[],reading:'I am Mei. I am from China, but I live in Bristol now. Ben is my colleague. He is Canadian and lives near our office. Our neighbour Rosa is from Brazil. Anna is French, but her home is in Bristol too. Tom is British. His colleague Max is German. On a registration form, nationality means the country you belong to. It does not always tell people where you live now. We often meet at a small café after work.',readQuestions:[
 ['Mei 现在住在哪里？',['Bristol','China','Canada'],0,'from 后面是来源，live in 后面是现居地。'],
 ['谁是 Mei 的同事？',['Anna','Ben','Rosa'],1,'Ben 是 colleague；Rosa 是 neighbour。'],
 ['Anna 是法国人，能否据此认定她现在住在法国？',['能，她一定住在法国','不能，她现在住在 Bristol','文章没有提到她住哪里'],1,'国籍与现居地是不同信息。French 说明国籍，后文说明现居地。']
 ],dialogues:[
 {text:'A: Hi, I am Sam. Are you from Canada? B: No, I am from Australia. I live in Leeds now. A: Nice to meet you.',q:'Sam 的交谈对象来自哪里？',options:['加拿大','澳大利亚','英国'],answer:1,why:'对方否定 Canada，并说明来自 Australia；Leeds 是现居地。'},
 {text:'A: Is Rosa your colleague? B: No, she is my neighbour. My colleague is Ben. He is Canadian.',q:'Rosa 与说话者是什么关系？',options:['同事','家人','邻居'],answer:2,why:'对方纠正 colleague，明确说是 neighbour。'},
 {text:'A: Are Anna and Max British? B: No. Anna is French, and Max is German. They both live in Bristol. A: So Bristol is their home now? B: Yes.',q:'哪一项符合对话？',options:['两个人现在都住在 Bristol','两个人都是英国人','Anna 是德国人'],answer:0,why:'听懂两个人的国籍后，还要保留共同的现居地信息。'}
 ]});
const b=lessons.find(x=>x.id==='u1b');Object.assign(b,{time:'16–22 分钟',recycle:['u1a-w6','u1a-w8'],reading:'Ben is Mei’s colleague. He is usually quiet at work, but he is friendly and helpful. He helps new colleagues find the right room. His cousin Leo is tall and likes talking to people. Leo is married. Ben is single and lives with a friend. Their neighbour Rosa works in a busy café. She is kind to the people who visit. Ben and Rosa study English together on Tuesday evenings. A quiet person can still be friendly; one adjective does not tell you everything about a person.',readQuestions:[
 ['Ben 安静，是否说明他不友好？',['是，quiet 就是不友好','不是，他也友好且乐于助人','文章没有描述他的性格'],1,'quiet 描述话多话少，friendly 描述待人的方式，两者可以同时成立。'],
 ['谁已经结婚？',['Ben','Rosa','Leo'],2,'Leo is married；Ben is single。注意人名与状态的对应。'],
 ['Ben 和 Rosa 什么时候一起学英语？',['周二晚上','每天上午','周日中午'],0,'together 表明一起，on Tuesday evenings 给出时间。']
 ]});
const c=lessons.find(x=>x.id==='u1c');Object.assign(c,{time:'18–25 分钟',recycle:['u1a-w1','u1a-w2','u1b-w1'],reading:'A friendly receptionist helps you join the English group. First name: Alex. Surname: Park. City: Bristol. Country: the United Kingdom. Home address: 18 King Street. Email: alex.park@example.com. Please check your first name, surname and address before you send the form. If a number is not correct, ask the receptionist to change it. You can say, “Could you repeat the number, please?” Then check it: “Is that eighteen King Street?” Do not guess a number that you have not heard clearly.',readQuestions:[
 ['登记表中 Alex 的姓是什么？',['Alex','King','Park'],2,'surname 是姓；King 是街名的一部分。'],
 ['发送表格前先做什么？',['检查姓名和地址','随意补一个号码','先离开接待处'],0,'before 表明检查在发送之前。没听清的信息要先问清楚。'],
 ['“Is that eighteen King Street?” 的主要作用是什么？',['介绍一个新国家','核对刚听到的信息','拒绝提供地址'],1,'Is that ...? 在这里用于确认听到的门牌号和街名。']
 ],dialogues:[
 {text:'A: What is your surname? B: Park. A: Could you spell that, please? B: P, A, R, K. A: Thank you.',q:'对方拼读字母是为了什么？',options:['核对姓氏','提供密码','介绍国家'],answer:0,why:'上下文正在登记 surname，请求 spell 是为了避免把姓听错。'},
 {text:'A: Is your address thirteen King Street? B: No, eighteen King Street. A: Eighteen? B: Yes, that is correct.',q:'最终确认的门牌号是多少？',options:['13','30','18'],answer:2,why:'最先出现的 thirteen 是错误猜测，eighteen 才是更正后的号码。'},
 {text:'A: Is your first name Park? B: No. Alex is my first name. Park is my surname. A: Alex Park? B: That is right.',q:'哪一项对应正确？',options:['名 Park，姓 Alex','名 Alex，姓 Park','姓 King，名 Alex'],answer:1,why:'对话先纠正 first name，再给出完整姓名确认。'},
 {text:'A: Could you repeat your city, please? B: Bristol. A: Is that in Canada? B: No. Bristol is in the United Kingdom. I am Canadian, but I live here now.',q:'哪一项符合最终确认的信息？',options:['住在英国的 Bristol，国籍加拿大','住在加拿大，国籍英国','住在英国，国籍未说明'],answer:0,why:'本题复用 1A：来源或国籍与现在居住的城市不能混在一起。'}
 ]});
const d=lessons.find(x=>x.id==='u1d');d.extraWords=d.words.splice(6);d.gap=['You can ___ me by email.',['interest','contact','language'],1,'contact someone by email 表示通过邮件联系某人。选学释义不作为必答核心填空。'];Object.assign(d,{time:'12–18 分钟',recycle:['u1a-w1','u1a-w2','u1b-w1','u1c-w4'],reading:'Hello, I am Mei. I am from China and live in Bristol. I work at a local library. My interests are music and walking. I study English in the evening because I want to meet people in my city. You can contact me by email about our English group. My profile shows my first name, city and interests. It does not show my home address or private phone number. Ben’s profile says he is Canadian and works near my library. He is friendly and helpful. His interests are films and music. Our profiles give useful information, but they do not tell readers everything about us.',readQuestions:[
 ['Mei 在哪里工作？',['当地的图书馆','一家音乐公司','一所语言学校'],0,'work at 后面是工作地点；music 是兴趣。'],
 ['Mei 和 Ben 有什么共同兴趣？',['走路','电影','音乐'],2,'Mei 的兴趣是 music and walking，Ben 的是 films and music。'],
 ['简介没有公开 Mei 的哪些信息？',['城市和兴趣','家庭地址和私人电话','名字和邮箱联系方式'],1,'does not show 后面是未公开的信息。未给出的信息不能自行推断。'],
 ['需要联系英语小组时应该怎么做？',['去找她的家庭住址','发邮件联系 Mei','先猜测她的电话号码'],1,'contact me by email 给出明确联系渠道，不能把 local library 当成家庭住址。']
 ],dialogues:[
 {text:'A: How can I contact you about the English group, Mei? B: Please send me an email. My home address is not on my profile. A: That is fine. I only need the group information.',q:'联系英语小组应使用什么渠道？',options:['邮箱','家庭地址','私人电话'],answer:0,why:'对方明确给出 email，其他渠道未提供。'},
 {text:'A: Your profile says you work in a shop. Is that correct? B: No. That is my old job. I work in a library now. A: Could you update the profile? B: Yes, thank you.',q:'Mei 的简介需要更正什么？',options:['她的兴趣','她的现居城市','她现在的工作'],answer:2,why:'shop 是旧工作，library 是现在的工作；本课整合 profile、job 与核对信息。'}
 ]});
for(const l of lessons){l.revision=l.unit===1?2:1;l.role='lesson';l.extraWords=l.extraWords||[];l.recycle=l.recycle||[];l.time=l.time||'12–18 分钟';l.planned=plan.units[l.unit-1].lessons.find(x=>x.id===l.id);}
const reviewText='New members of the English group. Noor is from Egypt and lives in Bristol. She works at a local library. Her interests are music and films. Ben is Canadian. He lives in Bristol too. He is quiet but friendly, and he studies English on Tuesday evenings. To contact the group, send an email to the receptionist. Members do not have to share their home addresses on their profiles.';
const checkText='Reception note. First name: Alex. Surname: Park. City: Bristol. Address: 18 King Street. Please check these details. If a name or number is wrong, ask the receptionist to repeat it. A form can contain an old job or a spelling mistake, so read the information carefully before you confirm it.';
lessons.push({id:'u1r',unit:1,code:'1R',title:'认识彼此 · 综合听读复习',goal:'在新的介绍、登记和人物资料里，综合判断来源、人物关系、联系方式和更正信息。',focus:'四课整合；from 的常用语境；本单元目标抽样复现',role:'unit-review',revision:2,time:'15–20 分钟',words:[],extraWords:[],recycle:[],noLanguageDrills:true,reading:reviewText,readQuestions:[
 ['Noor 现在住在哪里？',['埃及','Bristol','加拿大'],1,'from Egypt 是来源；lives in Bristol 是现居地。'],
 ['Noor 和 Ben 有什么共同点？',['国籍相同','兴趣完全一样','现在都住在 Bristol'],2,'两人的国籍不同，文章明确写他们都住在 Bristol。'],
 ['Ben 安静但友好，这种描述是否矛盾？',['不矛盾，两词描述不同方面','矛盾，安静就是不友好','无法理解 quiet'],0,'本题复用 1B 的人物描述，而非把一个词当成完整性格判断。'],
 ['联系英语小组应如何做？',['寻找成员家庭地址','向接待人员发邮件','猜一个电话号码'],1,'材料指定 email to the receptionist；家庭地址无需公开。'],
 ['登记表上 Alex 的姓是什么？',['Park','Alex','King'],0,'区分姓名字段与地址字段。',checkText],
 ['该表确认的门牌号是多少？',['13','30','18'],2,'地址字段为 18 King Street。',checkText],
 ['登记信息可能有误时，应该怎么做？',['直接确认，不再阅读','核对并请求重复错误信息','删除所有联系方式'],1,'本题整合 1C 的核对动作和 1D 的资料阅读。',checkText],
 ['“She is from Egypt.” 中 from 表示什么？',['来自；说明来源','现在住在','她的职业是'],0,'from 在这里说明来源，不能据此推断她现在的居住地。']
 ],dialogues:[
 {text:'A: Is Noor your colleague? B: No, she is my neighbour. We both work in libraries, but we work in different places.',q:'Noor 与说话者是什么关系？',options:['同事','家人','邻居'],answer:2,why:'工作类型相同并不意味着是同事；回答明确说 neighbour。'},
 {text:'A: Are you British, Ben? B: No, I am Canadian. I live in Bristol now. A: So your home is in Britain? B: Yes, that is right.',q:'哪一项符合 Ben 的情况？',options:['加拿大人，现居英国','英国人，现居加拿大','国籍和居住地都未说明'],answer:0,why:'在新情境中再次区分 nationality 与现居地。'},
 {text:'A: Your address is thirteen King Street. B: Could you repeat the number, please? A: Thirteen. B: That is not correct. It is eighteen. A: Eighteen King Street. B: That is right.',q:'最终应该记录哪个号码？',options:['13','18','30'],answer:1,why:'最后一次双方确认的信息为 eighteen，不是最先听见的 thirteen。'},
 {text:'A: Is Leo single? B: No, he is married. His cousin Ben is single. They are both in the English group. Leo comes on Wednesday and Ben comes on Tuesday.',q:'哪一项正确？',options:['Leo 单身，周二上课','Ben 已婚，周三上课','Leo 已婚，Ben 单身'],answer:2,why:'同时跟踪两个人的状态；不要把 cousin 和人物身份混淆。'}
 ]});
const unit2=typeof module==='object'&&module.exports?require('./unit2.js'):root.WORDCARD_UNIT2;
for(const l of unit2.lessons){l.planned=plan.units[1].lessons.find(p=>p.id===l.id);lessons.push(l);}
const books=JSON.parse(JSON.stringify(old.books));books[0].description='整册 12 单元／48 课规划；第一、二单元完整原创听读＋各自综合复习。';
const data={release:'empower-a2-unit2-20261031',version:'empower-unit1-expanded-2',units:plan.units,lessons,books,plan,copyright:old.copyright};
if(typeof module==='object'&&module.exports)module.exports=data;else root.WORDCARD_DATA=data;
})(typeof window==='object'?window:globalThis);
